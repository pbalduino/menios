#ifndef MENIOS_INCLUDE_KERNEL_TIMER_H
#define MENIOS_INCLUDE_KERNEL_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#define DIV_BY_2    0x00
#define DIV_BY_4    0x01
#define DIV_BY_8    0x02
#define DIV_BY_16   0x03
#define DIV_BY_32   0x08
#define DIV_BY_64   0x09
#define DIV_BY_128  0x0a
#define DIV_BY_1    0x0b

void timer_init();

uint64_t unix_time();

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_KERNEL_TIMER_H
