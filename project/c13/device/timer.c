#include "timer.h"
#include "io.h"
#include "print.h"
#include "../thread/thread.h"
#include "debug.h"
#include "interrupt.h"

#define IRQ0_FREQUENCY          100                                 // IRQ0 频率
#define INPUT_FREQUENCY         1193180                             // 8253input频率
#define COUNTER0_VALUE          INPUT_FREQUENCY / IRQ0_FREQUENCY    // IRQ0计数初值
#define COUNTER0_PORT           0x40                                // 计数器端口
#define COUNTER0_NO             0                                   // 控制字中使用的计数器号码
#define COUNTER_MODE            2                                   // 计数器工作方式
#define READ_WRITE_LATCH        3                                   // 读写方式, 先读写低8位, 再读写高8位
#define PIT_CONTROL_PORT        0x43                                // 控制字寄存器端口


uint32_t ticks; // ticks 是内核自中断开启以来总共的嘀嗒数

/* 时钟的中断处理函数 */
static void intr_timer_handler(void) {
    struct task_struct* cur_thread = running_thread();

    ASSERT(cur_thread->stack_magic == 0x19870916);      // 检查栈是否溢出

    cur_thread->elapsed_ticks++;        // 记录此线程占用的 cpu 时间
    ticks++;                            // 内核态和用户态总共的嘀嗒数

    if(cur_thread->ticks == 0) {
        // 若进程时间片用完, 就开始调度新的进程上 cpu
        schedule();
    } else {
        cur_thread->ticks--;
    }
}

/* 把操作的计数器counter_no、读写锁属性rwl、计数器模式counter_mode写入模式控制寄存器井赋予初始值counter_value */
static void frequency_set(uint8_t counter_port, 
                          uint8_t counter_no, 
                          uint8_t rwl, 
                          uint8_t counter_mode, 
                          uint16_t counter_value) {

    // 往控制字寄存器端口 0x43 写入控制字
    outb(PIT_CONTROL_PORT, (uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));

    // 先写入低 8 位
    outb(counter_port, (uint8_t) counter_value);

    // 再写入高8位
    outb(counter_port, (uint8_t) counter_value >> 8);
}


/* 初始化 PIT8253 */
void timer_init() {
    put_str("timer_init start\n");

    // 设置8253的定时周期, 即发送中断的周期
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done\n");
}

#define IRQ0_FREQUENCY	   100

#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)	// 每多少毫秒发生一次中断

// 以 tick 为单位的 sleep, 任何时间形式的 sleep 会转换此 ticks 形式
static void ticks_to_sleep(uint32_t sleep_ticks) {
   uint32_t start_tick = ticks;
   // 若间隔的 ticks 数不够便让出 cpu
   while (ticks - start_tick < sleep_ticks) {
      thread_yield();
   }
}

// 以毫秒为单位的 sleep
void mtime_sleep(uint32_t m_seconds) {
   // 计算要休眠的 ticks数 
   uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
   ASSERT(sleep_ticks > 0);
   ticks_to_sleep(sleep_ticks);
}



// /* 初始化 PIT8253 */
// void timer_init() {
//     put_str("timer_init start\n");

//     // 设置8253的定时周期, 即发送中断的周期
//     frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);

//     put_str("timer_init done\n");
// }

