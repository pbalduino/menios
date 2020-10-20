#ifndef MENIOS_INCLUDE_KERNEL_ISR_H
#define MENIOS_INCLUDE_KERNEL_ISR_H

#include <kernel/interrupts.h>

#define ISR_DOUBLE_FAULT 0x08
#define ISR_PAGE_FAULT   0x0e

void init_isr();

#endif
