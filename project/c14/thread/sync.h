#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "list.h"
#include "stdint.h"


/* 信号量结构 */
struct semaphore {
    uint8_t value;
    struct list waiters;    // 记录在此信号量上等待(阻塞)的所有线程
};


/* 锁结构 */
struct lock {
    struct task_struct* holder;     // 锁的持有者
    struct semaphore semaphore;     // 用二元信号量实现锁
    uint32_t holder_repeat_nr;      // 锁的持有者重复申请锁的次数
};

// 初始化信号量 
void sema_init(struct semaphore* psema, uint8_t value);

// 信号量 down 操作
void sema_down(struct semaphore* psema);

// 信号量的 up 操作
void sema_up(struct semaphore* psema);

// 初始化锁 plock
void lock_init(struct lock* plock);

// 获取锁 plock
void lock_acquire(struct lock* plock);

// 释放锁 plock
void lock_release(struct lock* plock);

#endif
