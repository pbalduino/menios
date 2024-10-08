#ifndef MENIOS_INCLUDE_KERNEL_TIMER_H
#define MENIOS_INCLUDE_KERNEL_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

void timer_init();
void timer_eoi();

uint64_t unix_time();

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_KERNEL_TIMER_H
