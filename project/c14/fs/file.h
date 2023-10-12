#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "stdint.h"
#include "../device/ide.h"
#include "dir.h"
#include "global.h"

// 文件结构
struct file {
    uint32_t fd_pos; // 记录当前文件操作的偏移地址, 以 0 为起始, 最大为文件大小 - 1
    uint32_t fd_flag;	// 文件操作标识
    struct inode* fd_inode;	// inode 指针
};

// 标准输入输出描述符
enum std_fd {
    stdin_no,  // 0 标准输入
    stdout_no, // 1 标准输出
    stderr_no  // 2 标准错误
};

// 位图类型
enum bitmap_type {
    INODE_BITMAP, // inode 位图
    BLOCK_BITMAP  // 块位图
};

#define MAX_FILE_OPEN 32 // 系统可打开的最大文件数

extern struct file file_table[MAX_FILE_OPEN];

/* 分配一个 i 结点, 返回 i 结点号 */
int32_t inode_bitmap_alloc(struct partition* part);

/* 分配 1 个扇区, 返回其扇区地址 */
int32_t block_bitmap_alloc(struct partition* part);

int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag);

/* 将内存中 bitmap 第 bit_idx 位所在的 512 字节同步到硬盘 */
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp);

/* 从文件表 file_table 中获取一个空闲位, 成功返回下标, 失败返回 -1 */
int32_t get_free_slot_in_global(void);

/* 将全局描述符下标安装到进程或线程自己的文件描述符数组 fd_table中,
 * 成功返回下标, 失败返回-1 */
int32_t pcb_fd_install(int32_t global_fd_idx);

/* 打开编号为 inode_no 的 inode 对应的文件, 若成功则返回文件描述符, 否则返回-1 */
int32_t file_open(uint32_t inode_no, uint8_t flag);

/* 从文件 file 中读取 count 个字节写入 buf, 返回读出的字节数, 若到文件尾则返回 -1 */
int32_t file_read(struct file* file, void* buf, uint32_t count);

/* 把 buf 中的 count 个字节写入 file, 成功则返回写入的字节数, 失败则返回 -1 */
int32_t file_write(struct file* file, const void* buf, uint32_t count);

/* 关闭文件 */
int32_t file_close(struct file* file);

/* 将文件描述符转化为文件表的下标 */
uint32_t fd_local2global(uint32_t local_fd);

/* 关闭文件描述符 fd 指向的文件, 成功返回 0, 否则返回 -1 */
int32_t sys_close(int32_t fd);

/* 把 buf 中的 count 个字节写入 file, 成功则返回写入的字节数, 失败则返回 -1 */
int32_t file_write(struct file* file, const void* buf, uint32_t count); 

#endif
