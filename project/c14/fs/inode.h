#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "stdint.h"
#include "list.h"
#include "../device/ide.h"

/* inode 结构 */
struct inode {
    uint32_t i_no;                  // inode 编号

    // 当此inode是文件时, i_size是指文件大小
    // 若此inode是目录时, i_size是指该目录下所有目录项大小之和
    uint32_t i_size;

    uint32_t i_open_cnts;           // 记录此文件被打开的次数
    bool write_deny;                // 写文件不能并行, 进程写文件前检查此标识

    // i_sectors[0-11]是直接块, i_sectors[12]用来存储一级间接块指针
    uint32_t i_sectors[13];
    struct list_elem inode_tag;     // 用于加入已打开的 inode 队列
                                    // 从硬盘读取速率太慢此 list 做缓冲用 当第二次使用时如果list中有
    					            // 直接通过 list_elem 得到 inode 而不用再读取硬盘
};

/* 根据 i 结点号返回相应的 i 结点 */
struct inode* inode_open(struct partition* part, uint32_t inode_no);

/* 将 inode 写入到分区 part */
void inode_sync(struct partition* part, struct inode* inode, void* io_buf);

/* 初始化 new_inode */
void inode_init(uint32_t inode_no, struct inode* new_inode);

/* 关闭 inode 或减少 inode 的打开数 */
void inode_close(struct inode* inode);

/* 回收 inode 的数据块和 inode 本身 */
void inode_release(struct partition* part, uint32_t inode_no);

#endif
