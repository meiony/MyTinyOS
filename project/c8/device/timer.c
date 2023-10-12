#include "timer.h"
#include "io.h"
#include "print.h"

#define IRQ0_FREQUENCY          100                                 // IRQ0 频率
#define INPUT_FREQUENCY         1193180                             // 8253input频率
#define COUNTER0_VALUE          INPUT_FREQUENCY / IRQ0_FREQUENCY    // IRQ0计数初值
#define COUNTER0_PORT           0x40                                // 计数器端口
#define COUNTER0_NO             0                                   // 控制字中使用的计数器号码
#define COUNTER_MODE            2                                   // 计数器工作方式
#define READ_WRITE_LATCH        3                                   // 读写方式, 先读写低8位, 再读写高8位
#define PIT_CONTROL_PORT        0x43                                // 控制字寄存器端口


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

    put_str("timer_init done\n");
}
