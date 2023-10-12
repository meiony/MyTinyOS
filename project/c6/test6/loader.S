%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR

    LOADER_STACK_TOP equ LOADER_BASE_ADDR           ; loader在保护模式下的栈指针地址，esp

    ; 构建GDT及其内部描述符, 每个描述符8个字节, 拆分为高低各4字节(32位)
    GDT_BASE:   dd 0x00000000                       ; 第0个描述符,不可用
                dd 0x00000000

    CODE_DESC:  dd 0x0000ffff                       ; 低32位31~16位为段基址15~0位, 15~0位为段界限15~0位
                dd DESC_CODE_HIGH4

    DATA_STACK_DESC:    dd 0x0000ffff               ; 数据段(栈段)描述符
                        dd DESC_DATA_HIGH4

    VIDEO_DESC: dd 0x80000007                       ; 0xB8000 到0xBFFFF为文字模式显示内存 段界限:limit=(0xbffff-0xb8000) / 4k=0x7
                dd DESC_VIDEO_HIGH4                 ; 0xB

    GDT_SIZE:   equ $ - GDT_BASE                    ; 当前位置减去GDT_BASE的地址 等于GDT的大小
    GDT_LIMIT:  equ GDT_SIZE - 1                    ; SIZE - 1即为最大偏移量

    times 60 dq 0                                   ; 预留60个 四字型 描述符空位, 用于后续扩展
                                                    
    SELECTOR_CODE  equ (0x0001<<3) + TI_GDT + RPL0  ; 段选择子: 低3位为TI RPL状态, 其余为描述符索引值
    SELECTOR_DATA  equ (0x0002<<3) + TI_GDT + RPL0  
    SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

    ; total_mem_bytes用于保存内存容量,以字节为单位,此位置比较好记。
    ; 当前偏移loader.bin文件头0x200字节,loader.bin的加载地址是0x900,
    ; 故total_mem_bytes内存中的地址是0xb00.将来在内核中咱们会引用此地址
    total_mem_bytes dd 0

    ; gdt指针, 前2字节为gdt界限, 后4字节为gdt起始地址(共48位)
    gdt_ptr dw GDT_LIMIT
            dd GDT_BASE

    ; 人工对齐:total_mem_bytes 4字节 + gdt_ptr 6字节 + ards_buf 244字节 + ards_nr 2字节 , 共256字节
    ards_buf times 244 db 0                         ; 记录内存大小的缓冲区
    ards_nr dw 0                                    ; 记录 ARDS 结构体数量


    loader_start:

        ; -------  int 15h eax = 0000E820h ,edx = 534D4150h ('SMAP') 获取内存布局  -------
        xor ebx, ebx                                ; 第一次调用将eb置0
        mov edx, 0x534d4150                         ; edx只赋值一次，循环体中不会改变
        mov di,  ards_buf                           ; di指向ARDS缓冲区

        .e820_mem_get_loop:                         ; 循环获取每个ARDS内存范围描述结构
            mov eax, 0x0000e820                     ; 执行int 0x15后,eax值变为0x534d4150, 所以每次执行int前都要更新为子功能号
            mov ecx, 20                             ; ARDS地址范围描述符结构大小是20字节
            int 0x15
            jc  .e820_failed_so_try_e801            ; 若cf位为1则有错误发生，尝试0xe801子功能
            add di, cx                              ; 使di增加20字节指向缓冲区中新的ARDS结构位置
            inc word [ards_nr]                      ; ARDS数量加1
            cmp ebx, 0                              ; 若 ebx 为 0 且 cf 不为 1, 这说明 ards 全部返回，当前已是最后一个
            jnz .e820_mem_get_loop                  ; 不为0则循环获取

            ; 在所有ards结构中，找出(base_add_low + length_low)的最大值，即内存的容量
            mov cx, [ards_nr]                       ; 遍历每一个ARDS结构体,循环次数是ARDS的数量
            mov ebx, ards_buf
            xor edx, edx                            ; 用edx记录最大值, 在这里先清零

        .find_max_mem_area:                         ; 无须判断type是否为1,最大的内存块一定是可被使用
            mov eax, [ebx]                          ; base_add_low
            add eax, [ebx+8]                        ; base_add_low + length_low = 这块ADRS容量
            add ebx, 20                             ; 指向下一块ARDS
            cmp edx, eax                            ; 找出最大,edx寄存器始终是最大的内存容量
            jge .next_ards                          ; 如果edx>=eax, 继续遍历下一块
            mov edx, eax                            ; 如果edx<=eax, 更新edx
        .next_ards:
            loop .find_max_mem_area
            jmp .mem_get_ok                         ; 获取内存容量结束


        ; ------  int 15h ax = E801h 获取内存大小,最大支持4G  ------
        ; 返回后, ax cx 值一样,以KB为单位,bx dx值一样,以64KB为单位
        ; 在ax和cx寄存器中为低16M,在bx和dx寄存器中为16MB到4G
        .e820_failed_so_try_e801:
            mov ax, 0xe801
            int 15
            jc .e801_failed_so_try88                ; 若当前e801方法失败,就尝试0x88方法

            ; 1 先算出低15M的内存, ax和cx中是以KB为单位的内存数量,将其转换为以byte为单位
            ; 乘数在eax, 即内存数量, 积高16位在edx, 低16位在eax
            mov cx, 0x400                           ; 0x400 = 1024
            mul cx
            shl edx, 16                             ; 左移16位, 将低16位放到edx高16位
            and eax, 0x0000FFFF                     ; 0x0000FFFF = 1111 1111 1111 1111, 高16位置0
            or  edx, eax                            ; 获得完整的32位积
            add edx, 0x100000                       ; edx比实际少1M, 故要加1MB
            mov esi, edx                            ; 先把低15MB的内存容量存入esi寄存器备份

            ; 2 再将16MB以上的内存转换为byte为单位,寄存器bx和dx中是以64KB为单位的内存数量
            xor eax, eax
            mov ax, bx
            mov ecx, 0x10000                        ; 0x10000十进制为64KB
            mul ecx                                 ; 32位乘法,默认的被乘数是eax, 积为64位, 高32位存入edx, 低32位存入eax
            add esi, eax                            ; 由于此方法只能测出4G以内的内存, 故32位eax足够了, edx肯定为0, 只加eax便可
            mov edx, esi                            ; edx为总内存大小
            jmp .mem_get_ok


        ; -----------------  int 15h ah = 0x88 获取内存大小,只能获取64M之内  ----------
        .e801_failed_so_try88:
            ; int 15后，ax存入的是以kb为单位的内存容量
            mov  ah, 0x88
            int  0x15
            jc   .error_hlt
            and  eax, 0x0000FFFF

            ; 16位乘法，被乘数是ax,积为32位, 积的高16位在dx中，积的低16位在ax中
            mov cx, 0x400                           ; 0x400等于1024, 将ax中的内存容量换为以byte为单位
            mul cx
            shl edx, 16
            or  edx, eax                            ; 把积的低16位组合到edx,为32位的积
            add edx, 0x100000                       ; 0x88子功能只会返回1MB以上的内存, 故实际内存大小要加上1MB


        .mem_get_ok:
            mov [total_mem_bytes], edx              ; 将内存换为byte单位后存入total_mem_bytes处


        ; --------------------------------- 设置进入保护模式 -----------------------------
        ; 1 打开A20 gate
        ; 2 加载gdt
        ; 3 将cr0 的 pe位(第0位)置1

        ; -----------------  打开A20  ----------------
        in al, 0x92                                 ; 端口号0x92 中的第1位变成 1 即可
        or al, 0000_0010B
        out 0x92, al

        ; -----------------  加载GDT  ----------------
        lgdt [gdt_ptr]

        ; -----------------  cr0第0位置1  ----------------
        mov eax, cr0
        or eax, 0x00000001
        mov cr0, eax


        ; -------------------------------- 已经打开保护模式 ---------------------------------------
        jmp dword SELECTOR_CODE:p_mode_start        ; 刷新流水线

        .error_hlt:                                 ; 出错则挂起
            hlt                                     ; 处理器暂停, 直到出现中断或复位信号才继续

        [bits 32]
        p_mode_start: 

            ; 初始化32位的段寄存器
            mov ax,  SELECTOR_DATA
            mov ds,  ax
            mov es,  ax
            mov ss,  ax
            mov esp, LOADER_STACK_TOP
            mov ax,  SELECTOR_VIDEO
            mov gs,  ax

            ;------------加载 kernel------------
            mov eax, KERNEL_START_SECTOR            ; kernel.bin所在的扇区号
            mov ebx, KERNEL_BIN_BASE_ADDR           ; 从硬盘读出后写入的地址
            mov ecx, 200                            ; 读入的扇区数

            call rd_disk_m_32                       ; 从硬盘读取文件到内存, 上面eax, ebx, ecx是参数


            ;------------启用 分页机制------------
            ; 创建页目录及页表并初始化页内存位图
            call setup_page

            ; gdt需要放在内核里
            ; 将描述符表地址&偏移量写入内存gdt_ptr，一会用新的地址加载
            sgdt [gdt_ptr]                          ; 取出GDT地址和偏移信息，存放在gdt_ptr这个内存位置上

            ; 视频段需要放在内核里与用户进程进行共享
            ; 将gdt描述符中视频段的段基址 + 0xc0000000
            mov ebx, [gdt_ptr + 2]                  ; 这里gdt_ptr前2字节是偏移量，后4字节是GDT基址，先选中GDT
            or dword [ebx + 0x18 + 4], 0xc0000000   ; 一个描述符8字节，0x18处是第3个段描述符也就是视频段, 修改段基址最高位为C, +4进入高4字节, 用or修改即可

            ; 将gdt的基址加上 0xc0000000 成为内核所在的地址
            add dword [gdt_ptr + 2], 0xc0000000
            add esp, 0xc0000000                     ; 将栈指针同样map到内核地址

            ; 页目录地址赋值给cr3
            mov eax, PAGE_DIR_TABLE_POS
            mov cr3, eax

            ; 打开 cr0 的 pg位 (第31位)
            mov eax, cr0
            or  eax, 0x80000000
            mov cr0, eax

            ; 开启分页后, 用 gdt 新的地址重新加载
            lgdt [gdt_ptr]


            ;;;;;;;;;;;;;;;;;;;;;;;;;;;;  此时不刷新流水线也没问题  ;;;;;;;;;;;;;;;;;;;;;;;;
            ; 由于一直处在32位下,原则上不需要强制刷新, 经过实际测试没有以下这两句也没问题.
            ; 但以防万一, 还是加上啦, 免得将来出来莫句奇妙的问题.
            jmp SELECTOR_CODE:enter_kernel          ; 强制刷新流水线，更新 gdt 


            enter_kernel:                           ; 进入内核
                call kernel_init
                mov esp, 0xc009f000                 ; 给栈选个高地址且不影响内存其他位置的地方
                jmp KERNEL_ENTRY_POINT


    ;-----------------   将kernel.bin中的segment拷贝到编译的地址   -----------
    ; 此时，kernel.bin 已经被读取到内存 KERNEL_BIN_BASE_ADDR 位置上了
    kernel_init:
        xor eax, eax                                ; 清空通用寄存器
        xor ebx, ebx                                ; ebx 记录程序头表文件内偏移地址，即e_phoff
        xor ecx, ecx                                ; cx  记录程序头表中的 program header 数量
        xor edx, edx                                ; dx  记录 program header 尺寸, 即 e_phentsize
        
        mov dx, [KERNEL_BIN_BASE_ADDR + 42]         ; 偏移文件 42 字节处是 e_phentsize, 表示 program header 大小
        mov ebx, [KERNEL_BIN_BASE_ADDR + 28]        ; 偏移文件 28 字节处是 e_phoff, 表示第一个程序头在文件的偏移量

        add ebx, KERNEL_BIN_BASE_ADDR               ; 获取程序头表第一个程序头的地址(基地址 + 偏移量)
        mov cx,  [KERNEL_BIN_BASE_ADDR + 44]        ; 偏移文件 44 字节处是 e_phnum，表示程序头的数量

        .each_segment:
            cmp byte [ebx + 0], PT_NULL             ; 若相等，则表示程序头没使用
            je  .PTNULL

            ; 为mem_cpy压入参数（从右往左）类似 memcpy(dst, src, size)
            ; 参数 size：待复制的大小
            push dword [ebx + 16]                   ; 偏移程序头 16 字节处是 p_filesz, 即本段在文件内的大小

            ; 参数 src: 源地址
            mov eax, [ebx + 4]                      ; 偏移程序头 4  字节处是 p_offset, 即本段在文件内的偏移大小
            add eax, KERNEL_BIN_BASE_ADDR           ; 加上基地址 = 物理地址
            push eax

            ; 参数 dst: 目的地址
            push dword [ebx + 8]                    ; 偏移程序头 8 字节处是 p_vaddr, 即本段在内存中的虚拟地址

            call .mem_cpy
            add esp, 12                             ; 清理栈中压入的三个参数


        .PTNULL:
            add ebx, edx                            ; 程序头的地址 + 程序头的大小 = 下一个程序头的地址
            loop .each_segment                      ; 复制下一个程序头
        
        ret


    ;----------  逐字节拷贝 mem_cpy(dst,src,size) ------------
    ; 输入:栈中三个参数(dst,src,size)
    ; 输出:无
    ;---------------------------------------------------------
    .mem_cpy:
        cld                                         ; 控制进行字符串操作时esi和edi的递增方式, cld增大, sld减小
        push ebp    
        mov ebp, esp
        push ecx                                    ; rep指令用到了ecx，外层指令也用到了ecx，所以备份

        ; 分析一下为什么是 8, 因为push了ebp, 所以相对应的都需要+4
        ; 并且进入函数时还 push了函数返回地址, 所以再+4, 所以一共+8
        mov edi, [ebp + 8]                          ; dst
        mov esi, [ebp + 12]                         ; src
        mov ecx, [ebp + 16]                         ; size
        rep movsb                                   ; 逐字节拷贝

        ; 恢复环境
        pop ecx
        pop ebp

        ret


    ; -------------   创建页目录及页表   ---------------
    setup_page:

        ; 先把页目录所占空间清 0
        mov ecx, 4096                               ; 1024 * 4 = 4096  
        mov esi, 0

        .clear_page_dir:
            mov byte [PAGE_DIR_TABLE_POS + esi], 0
            inc esi
            loop .clear_page_dir

        ; 开始创建页目录项（Page Directory Entry)
        .create_pde:
            mov eax, PAGE_DIR_TABLE_POS
            add eax, 0x1000                         ; 第一个页表的位置（仅次于页目录表，页目录表大小4KB）
            mov ebx, eax                            ; 0x00101 000, 储存到ebx为创建PTE时使用

            ; 下面将页目录项0和OxcOO都存为第一个页表的地址 ，每个页表表示4MB内存
            ; 这样Oxc03fffff(3G-3G04M)以下的地址和Ox003fffff(0-4M)以下的地址都 指向相同的页表
            ; 这是为将地址映射为内核地址做准备
            or eax,  PG_US_U | PG_RW_W | PG_P       ; 用户特权级，可读可写，存在内存
            mov [PAGE_DIR_TABLE_POS + 0x0], eax     ; 第一个目录项，0x00101 007
            mov [PAGE_DIR_TABLE_POS + 0xc00], eax   ; 第0xc00高10位0x300=768个页表占用的目录项，0xc00以上属于kernel空间
            ; 这里是把第768个目录页和第1个目录页指向同一个页表的物理地址：0x101000   
            ; 系统实际位于0～0x100000内存地址中，将系统虚拟地址0xc00000000映射到这低1M的空间内，只需要让0xc0000000的地址指向和低1M相同的页表即可

            sub eax, 0x1000
            mov [PAGE_DIR_TABLE_POS + 4092], eax    ; 使最后一个目录项指向页目录表自己的位置, 4092 = 1023 * 4

        
        ; 创建页表项（Page Table Entry）
        mov ecx, 256                                ; 1M低端内存/每页大小4K = 256
        mov esi, 0
        mov edx, PG_US_U | PG_RW_W | PG_P           ; edx中地址为0x0，属性为7，即111b

        .create_pte:
            mov  [ebx + esi * 4], edx               ; ebx = 0x00101 000, 即第一个PTE起始地址, 每个PTE = 4 byte
            add  edx, 4096                          ; edx + 4KB
            inc  esi
            loop .create_pte                        ; 低端1M内存中，物理地址=虚拟地址，这里创建了1M空间的页表项


        ; 创建内核其他页表的PDE
        mov eax, PAGE_DIR_TABLE_POS
        add eax, 0x2000                             ; eax指向第二个页表(每个页表对应一个PDE, 含有1024个页表项)
        or  eax, PG_US_U | PG_RW_W | PG_P
        mov ebx, PAGE_DIR_TABLE_POS                 
        mov ecx, 254                                ; 769~1022的所有目录项数量, 1022 - 769 + 1 = 254
        mov esi, 769                                

        .create_kernel_pde:
            mov  [ebx + esi * 4], eax
            inc  esi
            add  eax, 0x1000                        ; eax指向下一个页表
            loop .create_kernel_pde

        ret


    ;-------------------------------------------------------------------------------
	; 功能:读取硬盘n个扇区
    ; eax=LBA扇区号
	; ebx=将数据写入的内存地址
	; ecx=读入的扇区数   
    ;-------------------------------------------------------------------------------
	rd_disk_m_32:
        mov esi ,eax    ; 备份eax
        mov di ,cx      ; 备份cx

        ; 读写硬盘
        ; 1---设置要读取的扇区数
        mov dx ,0x1f2   ; 设置端口号，dx用来存储端口号的，要写入待读入的扇区数量
        mov al ,cl 
        out dx ,al      ; 读取的扇区数

        mov eax ,esi    ; 恢复eax


        ; 2---将LBA地址存入0x1f3～0x1f6
        ; LBA 7～0位写入端口0x1f3
        mov dx ,0x1f3
        out dx ,al 

        ; LBA 15~8位写入端口0x1f4
        mov cl ,8
        shr eax ,cl     ; 逻辑右移8位,将eax的最低8位移掉，让最低8位al的值变成接下来8位
        mov dx ,0x1f4
        out dx ,al 

        ; LBA 24～16位写入端口0x1f5
        shr eax ,cl 
        mov dx ,0x1f5
        out dx ,al 

        shr eax ,cl 
        and al ,0x0f    ; 设置lba 24～27位
        or al ,0xe0     ; 设置7～4位是1110表示LBA模式
        mov dx ,0x1f6
        out dx ,al 

        ; 3---向0x1f7端口写入读命令0x20
        mov dx ,0x1f7
        mov al ,0x20
        out dx ,al

        ; 4---检测硬盘状态
        .not_ready:
            ; 同写入命令端口，读取时标示硬盘状态，写入时是命令
            nop
            in al ,dx
            and al ,0x88    ; 第三位为1表示已经准备好了，第7位为1表示硬盘忙
            cmp al ,0x08
            jnz .not_ready

        ; 5---0x1f0端口读取数据
        mov ax ,di      ; 要读取的扇区数
        mov dx ,256     ; 一个扇区512字节，一次读取2字节，需要读取256次
        mul dx          ; 结果放在ax里
        mov cx ,ax      ; 要读取的次数

        mov dx ,0x1f0

        .go_on_read:
            in ax, dx
            mov [ebx], ax    ; bx是要读取到的内存地址
            add ebx, 0x02
            loop .go_on_read        ; 循环cx次

        ret
							 
