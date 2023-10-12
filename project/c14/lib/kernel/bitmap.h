#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H

#include "global.h"
#define BITMAP_MASK 1

struct bitmap {
    uint32_t btmp_bytes_len;
    /* 在遍历位图时, 整体上以字节为单位, 细节上是以位为单位, 所以此处位图的指针必须是单字节 */
    // 使用位图数组需要知道其长度, 但是长度得以后才能知道, 所以这里可以用指定地址来代替使用数组
    uint8_t* bits;
};

/* 将位图btmp初始化 */
void bitmap_init(struct bitmap* btmp);

/* 判断 bit_idx 位是否为 1 ,若为 1, 则返回true, 否则返回false */ 
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);

/* 在位图中申清连续 cnt 个位, 成功, 则返回其起始位下标, 失败, 返回 -1 */
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);

/* 将位图 btmp 的 bit_idx 位设置为 value */
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);

#endif
