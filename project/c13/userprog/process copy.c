#include "process.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
//#include "../thread/thread.h"
#include "list.h"
#include "tss.h"
#include "../kernel/interrupt.h"
#include "string.h"
#include "../device/console.h"

extern void intr_exit(void);        // 通过中断返回指令进入3特权级


/* 构建用户进程初始上下文信息(填充用户进程的 intr_stack) */
void start_process(void* filename_) {
    void* function = filename_;
    struct task_struct* cur = running_thread();         // 获取当前线程
    cur->self_kstack += sizeof(struct thread_stack);    // 指向 intr_stack
    // 用新变量保存 intr_stack 地址
    struct intr_stack* proc_stack = (struct intr_stack*) cur->self_kstack;
    proc_stack->edi = 0;                                // 初始化通用寄存器
    proc_stack->esi = 0;
    proc_stack->ebp = 0;
    proc_stack->esp_dummy = 0;

    proc_stack->ebx = 0;
    proc_stack->edx = 0;
    proc_stack->ecx = 0;
    proc_stack->eax = 0;

    proc_stack->gs = 0;                                 // 用户态用不上, 直接初始为 0
    proc_stack->ds = SELECTOR_U_DATA;                   // 选择子设置为3特权的区域
    proc_stack->es = SELECTOR_U_DATA;
    proc_stack->fs = SELECTOR_U_DATA;
    proc_stack->eip = function;                         // 待执行的用户程序地址
    proc_stack->cs = SELECTOR_U_CODE;                   // 修改 CS 段特权级
    // 修改 eflags 的 IF/IOPL/MBS
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);

    // 申请用户栈空间, 申请函数返回的是申请内存的下边界, 
    // 所以这里的地址(USER_STACK3_VADDR)应该是用户栈的下边界, 所以 + PG_SIZE得到栈底地址
    proc_stack->esp = (void*) ((uint32_t) get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);
    proc_stack->ss = SELECTOR_U_DATA;                   // 修改 SS 段特权级

    // 修改 esp 指针, 执行 intr_exit, 将 proc_stack 中的数据载入CPU寄存器, 进入特权级3
    asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(proc_stack) : "memory");
}

/* 激活页表(更新 cr3 指向的页目录表, 每个进程有自己的页目录表) */
void page_dir_activate(struct task_struct* p_thread) {
    /********************************************************
    * 执行此函数时, 当前任务可能是线程。
    * 之所以对线程也要重新安装页表, 原因是上一次被调度的可能是进程,
    * 否则不恢复页表的话, 线程就会使用进程的页表了。
    ********************************************************/

    // 若为内核线程, 需要重新填充页表为 0x100000
    // 默认为内核的页目录物理地址, 也就是内核线程所用的页目录表
    uint32_t pagedir_phy_addr = 0x100000;
    
    if(p_thread->pgdir != NULL) {
        // 用户态进程有自己的页目录表(每个页目录表代表4GB = 1024 * 4MB), 根据页表虚拟地址获得物理地址
        pagedir_phy_addr = addr_v2p((uint32_t) p_thread->pgdir);
    }

    // 更新页目录寄存器cr3, 使新页表生效
    asm volatile("movl %0, %%cr3" : : "r"(pagedir_phy_addr) : "memory");
}


/* 激活线程或进程的页表, 更新 tss 中的 esp0 为进程的特权级0的栈 */
void process_activate(struct task_struct* p_thread) {
    ASSERT(p_thread != NULL);
    // 激活该进程或线程的页表(更新 cr3 指向的页目录表)
    page_dir_activate(p_thread);

    // 内核线程特权级本身就是 0, 处理器进入中断时并不会从
    // tss 中获取 0 特权级栈地址, 故不需要更新 esp0
    if(p_thread->pgdir) {
        // 更新该进程的 esp0, 用于此进程被中断时保留上下文
        update_tss_esp(p_thread);
    }
}

/* 创建页目录表, 将当前页表的表示内核空间的 pde 复制,
 * 成功则返回页目录的虚拟地址, 否则返回 -1 */
uint32_t* create_page_dir(void) {
    // 用户进程的页表不能让用户直接访问到, 所以在内核空间来申请
    // 申请作为用户页目录表的基地址
    uint32_t* page_dir_vaddr = get_kernel_pages(1);     
    if(page_dir_vaddr == NULL) {
        console_put_str("create_page_dir: get_kernel_pages failed!");
        return NULL;
    }

    // 1. 先复制页表(将当前页表的表示内核空间的 pde 复制到新页表对应映射内核部分)
    // page_dir_vaddr + 0x300 * 4 是内核页目录的第 768 项
    // 0xfffff000 是内核页目录表的基地址, 会访问到当前页目录表的最后一个目录项, 也就是当前页目录表本身
    memcpy((uint32_t*) ((uint32_t) page_dir_vaddr + 0x300 * 4), (uint32_t*) (0xfffff000 + 0x300 * 4), 1024);

    // 2. 更新页目录地址
    // 得到新页目录的物理地址
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t) page_dir_vaddr);
    // 页目录地址是存入在页目录的最后一项
    // 更新页目录地址为新页目录的物理地址
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;

    return page_dir_vaddr;
}


/* 创建用户进程虚拟地址位图 */
void create_user_vaddr_bitmap(struct task_struct* user_prog) {
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;   // 提前定好的起始位置

    // 除法向上取整, 计算位图需要的内存页数
    // (内核起始地址 - 用户进程起始地址) / PG_SIZE / 8 = 位图需要的 byte
    // 再 / PG_SIZE 向上取整得到位图需要的内存页数
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);  

    // 给位图分配空间
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    
    //计算位图长度, 内存大小 / 页大小（1位=1页）/ 8位（1字节=8位）
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;

    // 将位图初始化
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}


/* 创建用户进程 */
void process_execute(void* filename, char* name) {
    // pcb 内核的数据结构, 由内核来维护进程信息, 因此要在内核内存池中申请
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, default_prio);        // 初始化进程基本信息
    create_user_vaddr_bitmap(thread);               // 创建用户进程虚拟地址位图
    // 初始化线程栈 thread_stack, start_process为进程待执行函数, filename为其参数
    // start_process: 填充用户进程的 intr_stack
    thread_create(thread, start_process, filename);
    thread->pgdir = create_page_dir();              // 创建进程的页目录表

    enum intr_status old_status = intr_disable();   // 关中断
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    // 将进程加入就绪队列
    list_append(&thread_ready_list, &thread->general_tag);  

    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    // 将进程加入所有任务队列
    list_append(&thread_all_list, &thread->all_list_tag);
    intr_set_status(old_status);
}

