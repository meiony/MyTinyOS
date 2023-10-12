#include "syscall-init.h"
#include "../lib/user/syscall.h"
#include "stdint.h"
#include "print.h"
#include "../thread/thread.h"
#include "../device/console.h"
#include "string.h"
#include "../kernel/memory.h"
#include "../fs/fs.h"

#define syscall_nr 32   // 最大支持的系统子功能调用数
typedef void* syscall;

syscall syscall_table[syscall_nr];


/* 返回当前任务的 pid */
uint32_t sys_getpid(void) {
    return running_thread()->pid;
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
