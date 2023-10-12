#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "stdint.h"
#include "bitmap.h"

#define PG_P_1 1    //页表项或页目录项存在属性位1
#define PG_P_0 0    //页表项或页目录项存在属性位0
#define PG_RW_R 0   //R/W属性位，读/执行    00
#define PG_RW_W 2   //RW属性位，读/写/执行  10
#define PG_US_S 0   //U/S属性位，系统级     000
#define PG_US_U 4   //U/S 属性位，用户级    100


/* 用于虚拟地址管理 */
struct virtual_addr {
    struct bitmap vaddr_bitmap;     // 虚拟地址用到的位图结构
    uint32_t vaddr_start;           // 虚拟地址起始地址
};

/* 内存池标记， 用于判断用哪个内存池*/
enum pool_flags {
    PF_KERNEL = 1,  //内核物理内存池
    PF_USER = 2     //用户物理内存池
};

extern struct pool kernel_pool, user_pool;

void mem_init(void);

/* 从内核物理内存池中申请 pg_cnt 页内存, 成功则返回其虚拟地址, 失败则返回 NULL */
void* get_kernel_pages(uint32_t pg_cnt);

/* 分配 pg_cnt 个页空间, 成功则返回起始虚拟地址, 失败时返回 NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);

void malloc_init(void);

/* 得到虚拟地址vaddr对应的pte指针 */
uint32_t* pte_ptr(uint32_t vaddr);

/* 得到虚拟地址vaddr对应的pde指针 */
uint32_t* pde_ptr(uint32_t vaddr);

#endif
