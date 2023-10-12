#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"
#include "ide.h"

#define MAX_FILES_PER_PART  4096    // 每个分区所支持最大创建的文件
#define BITS_PER_SECTOR     4096    // 每扇区的位数 = 512字节 * 8
#define SECTOR_SIZE         512     // 扇区字节大小
#define BLOCK_SIZE  SECTOR_SIZE     // 块字节大小
#define MAX_PATH_LEN 512	        // 路径最大长度

/* 文件类型 */
enum file_types {
    FT_UNKNOWN,     // 不支持的文件类型
    FT_REGULAR,     // 普通文件
    FT_DIRECTORY    // 目录
};


/* 打开文件的选项 */
enum oflags {
    O_RDONLY,       // 只读
    O_WRONLY,       // 只写
    O_RDWR,         // 读写
    O_CREAT = 4     // 创建
};

/* 文件读写位置偏移量 */
enum whence {
    SEEK_SET = 1,   // 文件开始
    SEEK_CUR,       // 当前读写位置
    SEEK_END        // 文件最后一个字节的下一个字节
};

/* 重置用于文件读写操作的偏移指针, 成功时返回新的偏移量, 出错时返回 -1 */    
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);    



/* 用来记录查找文件过程中已找到的上级路径, 也就是查找文件过程中"走过的地方" */
struct path_search_record {
    char searched_path[MAX_PATH_LEN];   // 查找过程中的父路径
    struct dir* parent_dir;             // 文件或目录所在的直接父目录
    enum file_types file_type;          // 文件类型
};


extern struct partition* cur_part;
void filesys_init(void);

/* 返回路径深度, 比如/a/b/c, 深度为3 */
int32_t path_depth_cnt(char* pathname);

int32_t sys_open(const char* pathname, uint8_t flags);

/* 将 buf 中连续 count 个字节写入文件描述符 fd, 成功则返回写入的字节数, 失败返回 -1 */
int32_t sys_write(int32_t fd, const void* buf, uint32_t count);

/* 从文件描述符 fd 指向的文件中读取 count 个字节到 buf, 若成功则返回读出的字节数, 到文件尾则返回 -1 */
int32_t sys_read(int32_t fd, void* buf, uint32_t count);

/* 删除文件(非目录), 成功返回 0, 失败返回 -1 */
int32_t sys_unlink(const char* pathname);

/* 创建目录 pathname, 成功返回 0, 失败返回 -1 */
int32_t sys_mkdir(const char* pathname);

/* 目录打开成功后返回目录指针, 失败返回 NULL */
struct dir* sys_opendir(const char* name);

/* 成功关闭目录 dir 返回 0, 失败返回 -1 */
int32_t sys_closedir(struct dir* dir);

/* 读取目录 dir 的1个目录项, 成功后返回其目录项地址, 到目录尾时或出错时返回 NULL */
struct dir_entry* sys_readdir(struct dir* dir);

/* 把目录 dir 的指针 dir_pos 置 0 */
void sys_rewinddir(struct dir* dir);

/* 删除空目录, 成功时返回 0, 失败时返回 -1*/
int32_t sys_rmdir(const char* pathname);

/* 把当前工作目录绝对路径写入 buf, size是 buf 的大小.
 * 当 buf 为 NULL 时, 由操作系统分配存储工作路径的空间并返回地址
 * 失败则返回 NULL */
char* sys_getcwd(char* buf, uint32_t size);

/* 更改当前工作目录为绝对路径 path, 成功则返回 0,失败返回 -1 */
int32_t sys_chdir(const char* path) ;

// 文件属性结构体
struct stat {
    uint32_t st_ino; // inode 编号
    uint32_t st_size; // 尺寸
    enum file_types st_filetype; // 文件类型
};

int32_t sys_stat(const char* path, struct stat* buf);    

#endif
