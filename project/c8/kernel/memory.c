#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "global.h"
#include "debug.h"
#include "string.h"

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)   //返回虚拟地址高10位，用于定位pde
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)   //返回虚拟地址中间10位，用于定位pte

#define PG_SIZE 4096    // 页的大小: 4k


/************************** 位图地址 *******************************************
 * 因为 0xc009f000 是内核主线程栈顶, 0xc009e000 是内核主线程的pcb(pcb占用1页 = 4k).
 * 一个页框大小的位图可表示128M内存, 位图位置安排在地址0xc009a000,
 * 这样本系统最大支持4个页框的位图, 即512M
******************************************************************************/
#define MEM_BITMAP_BASE 0xc009a000


/* 0xc0000000是内核从虚拟地址3G起. 0x100000意指跨过低端1M内存, 使虚拟地址在逻辑上连续 */
#define K_HEAP_START 0xc0100000

/* 内存池结构, 生成两个实例用于管理内核内存池和用户内存池 */
struct pool {
    struct bitmap pool_bitmap;      // 本内存池用到的位图结构, 用于管理物理内存
    uint32_t phy_addr_start;        // 本内存池所管理物理内存的起始地址
    uint32_t pool_size;             // 本内存池字节容量
};

struct pool kernel_pool, user_pool; // 生成内核物理内存池和用户物理内存池
struct virtual_addr kernel_vaddr;   // 此结构用来给内核分配虚拟地址


/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem) {
    put_str("    mem_pool_init start\n");

    // 页表大小 ＝ 1页的页目录表 ＋第 0 和第 768 个页目录项指向同一个页表, 之前创建页表的时候, 挨着页目录表创建了768-1022总共255个页表 + 上页目录的1页大小, 就是256
    // 第 769~1022 个页目录项共指向 254 个页表, 共 256 个页框
    uint32_t page_table_size = PG_SIZE * 256;           // 记录页目录表和页表占用的字节大小

    uint32_t used_mem = page_table_size + 0x100000;     // 当前已经使用的内存字节数, 1M部分已经使用了, 1M往上是页表所占用的空间
    uint32_t free_mem = all_mem - used_mem;             // 剩余可用内存字节数
    uint16_t all_free_pages = free_mem / PG_SIZE;       // 所有可用的页
    // 1页为 4KB, 不管总内存是不是 4k 的倍数, 对于以页为单位的内存分配策略, 不足 1 页的内存不用考虑了

    uint16_t kernel_free_pages = all_free_pages / 2;    // 分配给内核的空闲物理页
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;

    // 为简化位图操作, 余数不处理, 坏处是这样做会丢内存。好处是不用做内存的越界检查, 因为位图表示的内存少于实际物理内存。
    uint32_t kbm_length = kernel_free_pages / 8;        // Kernel Bitmap的长度, 位图中的一位表示一页, 以字节为单位, 也就是8页表示1字节的位图
    uint32_t ubm_length = user_free_pages / 8;          // User Bitmap 的长度

    uint32_t kp_start = used_mem;                                   // kernel pool start, 内核内存池起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;     // 内核已使用的 + 没使用的, 就是分配给内核的全部内存, 剩下给用户

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;            // 内存池里存放的是空闲的内存, 所以用可用内存大小填充
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

    /*********    内核内存池和用户内存池位图   ***********
    *   位图是全局的数据, 长度不固定。
    *   全局或静态的数组需要在编译时知道其长度，
    *   而我们需要根据总内存大小算出需要多少字节。
    *   所以改为指定一块内存来生成位图.
    *   ************************************************/
    // 内核使用的最高地址是0xc009f000, 这是主线程的栈地址.(内核的大小预计为70K左右)
    // 32M内存占用的位图是2k. 内核内存池的位图先定在 MEM_BITMAP_BASE(0xc009a000)处.

    kernel_pool.pool_bitmap.bits = (void*) MEM_BITMAP_BASE;
    /* 用户内存池的位图紧跟在内核内存池位图之后 */
    user_pool.pool_bitmap.bits = (void*) (MEM_BITMAP_BASE + kbm_length);

    // 输出内存池信息
    put_str("        kernel_pool_bitmap_start: ");
    put_int((int) kernel_pool.pool_bitmap.bits);

    put_str(" kernel_pool_phy_addr_start: ");
    put_int(kernel_pool.phy_addr_start);

    put_str("\n");

    put_str("        user_pool_bitmap_start: ");
    put_int((int) user_pool.pool_bitmap.bits);

    put_str(" user_pool_phy_addr_start: ");
    put_int(user_pool.phy_addr_start);

    put_str("\n");

    // 将位图置 0
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    // 下面初始化内核虚拟地址的位图, 按实际物理内存大小生成数组
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
    // 用于维护内核堆的虚拟地址, 所以要和内核内存池大小一致

    // 位图的数组指向一块没用的内存, 目前定位在内核内存池和用户内存池之外
    kernel_vaddr.vaddr_bitmap.bits = (void*) (MEM_BITMAP_BASE + kbm_length + ubm_length);

    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);

    put_str("    mem_pool_init done \n");
}

// 内存管理部分初始化入口
void mem_init() {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*) (0xb00));  // 获取物理内存大小
    mem_pool_init(mem_bytes_total);                     // 初始化内存池
    put_str("mem_init done\n");
}

/*
*   再pf表示得虚拟内存池中申请pg_cnt 个虚拟页
*   成功则返回虚拟页得起始地址，失效则返回NULL
*/
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if (pf == PF_KERNEL) {
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);    //获取申请的虚拟页的位起始值
        if (bit_idx_start == -1) {
            return NULL;
        }

        //将位起始值开始连续置为1，直到设置完需要的页位置
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt, 1);
            cnt++;
        }
        //获取起始页的虚拟地址
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    } else {
        //用户内存池，将来实现用户进程时再补充
    }

    return (void*) vaddr_start;
}

/* 得到虚拟地址vaddr计算得到对应的pte指针（虚拟地址）*/
uint32_t* pte_ptr(uint32_t vaddr) {
    // 先访问到页表自己
    // 再用页目录项 pde（页目录内页表的索引）作为pte的索引访问到页表
    // 再用pte的索引作为页内偏移

    // 第一步：0xffc00000 是取出第1023个页目录项进行索引, 其实就是页目录表的物理地址
    // 第二步：((vaddr & 0xffc00000) >> 10) 是将原来vaddr的前10位取出, 放在中间10位的位置上 用来获取 pte 的
    // 第三步：PTE_IDX(vaddr) * 4 会被当作物理偏移直接加上, 而不会像其前面10位会被cpu自动*4再加上, 所以这里手动*4, 获取PTE索引, 得到PTE物理地址
    uint32_t* pte = (uint32_t*) (0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

/* 得到虚拟地址vaddr计算得到对应的pde的指针(虚拟地址) */
uint32_t* pde_ptr(uint32_t vaddr) {
    // 0xfffff 用来访问到页表本身所在的地址
    // 前10位是1023, 是页目录表的物理地址
    // 中10位是1023, 索引到的还是页目录表的物理地址
    // 后12位是addr的前10位*4, 也就是页目录表的索引
    uint32_t* pde = (uint32_t*) ((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

/* 
 * 在 m_pool 指向的物理内存池中分配 1 个物理页,
 * 成功则返回页框的物理地址, 失败则返回 NULL 
 * */
static void* palloc(struct pool* m_pool) {
    // 扫描或设置位图要保证原子操作
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);     // 找一个物理页面, 位图中1位表示实际1页地址
    if (bit_idx == -1) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);           // 将此位的 bit_idx 置 1  
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start); // 物理内存池起始地址 + 页偏移 = 页地址
    return (void*) page_phyaddr;
}





/* 页表中添加虚拟地址 _vaddr 与物理地址 _page_phyaddr 的映射 */
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
    uint32_t vaddr = (uint32_t) _vaddr;
    uint32_t page_phyaddr = (uint32_t) _page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);

    /************************   注意   **************************************************
    * 执行*pte, 会访问到空的pde。所以确保 pde 创建完成后才能执行 *pte,
    * 否则会引发page_fault。因此在 *pde 为0时, *pte 只能出现在下面 else 语句块中的* pde 后面。
    * ***********************************************************************************/

    // 先在页目录内判断目录项的p位, 若为1, 则表示该表已存在
    if (*pde & 0x00000001) {
        // 页目录项和页表项的第0位为P, 此处判断目录项是否存在
        ASSERT(!(*pte & 0x00000001));       // 此时pte应该不存在

        // 只要是创建页表, pte就应该不存在, 多判断一下放心
        if (!(*pte & 0x00000001)) {
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);     // 创建pte
        
        } else {
            // 目前执行不到这里
            PANIC("pte repeat");
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }

    } else {
        // 页目录项不存在, 所以要先创建页目录项再创建页表项
        // 页表中用到的页框一律从内核空间分配
        // pte 和 pde 都是真实的 pde 和 pte物理地址对应的虚拟地址 
        // pde_phyaddr是物理地址
        uint32_t pde_phyaddr = (uint32_t) palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

        /* 分配到的物理页地址 pde_phyaddr 对应的物理内存清 0,
        * 避免里面的陈旧数据变成了页表项, 从而让页表混乱.
        * 访问到 pde 对应的物理地址, 用 pte 取高20位便可.
        * 因为 pte 是基于该 pde 对应的物理地址再寻址,
        * 把低12位置0便是该pde对应的物理页的起始"虚拟地址"
        * */

        // 把分配到的物理页地址 pde_phyaddr(物理地址) 对应的物理内存清 0
        // (int) pte & 0xfffff000: vaddr 所在页表的虚拟地址, 即 pde_phyaddr 的虚拟地址
        memset((void*) ((int) pte & 0xfffff000), 0, PG_SIZE);

        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}


/* 分配 pg_cnt 个页空间, 成功则返回起始虚拟地址, 失败时返回 NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);

    /***********  malloc_page 的原理是三个动作的合成:   ***********
         1. 通过 vaddr_get 在虚拟内存池中申请虚拟地址
         2. 通过 palloc 在物理内存池中申请物理页
         3. 通过 page_table_add 将以上得到的虚拟地址和物理地址在页表中完成映射
   ***************************************************************/
    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == NULL) {
        return NULL;
    }

    uint32_t vaddr = (uint32_t) vaddr_start;
    uint32_t cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    // 因为虚拟地址是连续的, 但物理地址不连续, 所以逐个映射
    while ((cnt--) > 0) {
        void* page_phyaddr = palloc(mem_pool);
        if (page_phyaddr == NULL) {
            // 失败时要将曾经已申请的虚拟地址和
            // 物理页全部回滚, 在将来完成内存回收时再补充
            return NULL;
        }
        page_table_add((void*) vaddr, page_phyaddr);    // 在表中逐个做映射
        vaddr += PG_SIZE;                               // 下一个虚拟页
    }
    return vaddr_start;
}


/* 从内核物理内存池中申请 pg_cnt 页内存, 成功则返回其虚拟地址, 失败则返回 NULL */
void* get_kernel_pages(uint32_t pg_cnt) {
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        // 若分配的地址不为空, 将页框清 0 后返回
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    return vaddr;
}
