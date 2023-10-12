#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "../device/timer.h"
#include "memory.h"
#include "../thread/thread.h"
#include "../device/console.h"
#include "../device/keyboard.h"

/* 负责初始化所有模块 */
void init_all() {
    put_str("init_all\n");
    idt_init();         // 初始化中断
    mem_init();         // 初始化内存管理系统
    thread_init();      // 初始化线程相关结构
    timer_init();       // 初始化 PIT
    console_init();     // 初始化终端
    keyboard_init();    // 键盘初始化
}
