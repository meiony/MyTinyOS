#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

/* 将位图btmp初始化 */
void bitmap_init(struct bitmap* btmp) {
    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}


/* 判断 bit_idx 位是否为 1 ,若为 1, 则返回true, 否则返回false */ 
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {
    uint32_t byte_idx = bit_idx / 8;                          // 向下取整用于索引数组下标
    uint32_t bit_odd = bit_idx % 8;                           // 取余用于索引数组内的位
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd)); // 00x00000, 判断第x位是否为1
}


/* 在位图中申清连续 cnt 个位, 成功, 则返回其起始位下标, 失败, 返回 -1 */
int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {
    uint32_t idx_byte = 0;  // 用于记录空闲位所在的字节
    /* 先逐字节比较，蛮力法 */
    // 一位16进制代表4位二进制, 所以 0xff 代表一字节且该字节全为 1
    while ((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)) {
        idx_byte++;
    }

    /* 如果找不到可用空间就返回 -1 */
    ASSERT(idx_byte < btmp->btmp_bytes_len);
    if (idx_byte == btmp->btmp_bytes_len) {
        return -1;
    }

    /* 若在位图数组范围内的某字节内找到了空闲位, 在该字节内逐位比对, 返回空闲位的索引。*/
    int idx_bit = 0;
    while ((uint8_t) (BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) {
        idx_bit++;
    }

    int bit_idx_start = idx_byte * 8 + idx_bit; // 空闲位在位图中的下标
    if (cnt == 1) {
        return bit_idx_start;
    }

    uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start); // 记录还有多少位可以判断
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;                                             // 记录找到空闲位的个数

    bit_idx_start = -1;                                             // 先将其置为-1, 若找不到连续的位就返回-1
    while ((bit_left--) > 0) {
        if (!bitmap_scan_test(btmp, next_bit)) {
            // 若 next_bit为 0
            count++;
        } else {
            // 空间不连续, 重置连续空间数为0, 继续寻找连续空间
            count = 0;
        }

        // 若找到连续的 cnt 个空位
        if (count == cnt) {
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}


/* 将位图 btmp 的 bit_idx 位设置为 value */
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {
    ASSERT((value == 0) || (value == 1));
    uint32_t byte_idx = bit_idx / 8;    // 向下取整用于索引数组下标
    uint32_t bit_odd = bit_idx % 8;     // 取余用于索引数组内的位

    if (value) {
        // 如果 vaule 为 1, 按位或
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    } else {
        // 如果 vaule 为 0, 取反再按位与
        // 取反: 将除所要操作的位置 0, 其余位全部置 1
        // 按位与: 1 & 1 = 1, 0 & 1 = 0, 即其余位不受影响, 目标位置 0
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}
