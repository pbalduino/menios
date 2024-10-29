#ifndef MENIOS_INCLUDE_KERNEL_TSC_H
#define MENIOS_INCLUDE_KERNEL_TSC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <types.h>

#define TICKS_PER_SECOND 1000000000

void init_tsc();

uint64_t read_tsc(void);

useconds_t unix_time_us();

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_KERNEL_TSC_H