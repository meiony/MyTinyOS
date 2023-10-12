#include "syscall-init.h"
#include "../lib/user/syscall.h"
#include "stdint.h"
#include "print.h"
#include "../thread/thread.h"
#include "string.h"
#include "console.h"
#include "memory.h"


#define syscall_nr 32   // 最大支持的系统子功能调用数
typedef void* syscall;

syscall syscall_table[syscall_nr];


/* 返回当前任务的 pid */
uint32_t sys_getpid(void) {
    return running_thread()->pid;
}

/* 打印字符串str (未实现文件系统前的版本)*/
uint32_t sys_write(char* str) {
    console_put_str(str);   //在终端输出字符串str
    return strlen(str);
}

/* 初始化系统调用 */
void syscall_init(void) {
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    put_str("syscall_init done\n");
}
