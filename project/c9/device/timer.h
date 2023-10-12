#ifndef __DEVICE_TIME_H
#define __DEVICE_TIME_H
#include "stdint.h"

void timer_init(void);
static void intr_timer_handler(void);

#endif
