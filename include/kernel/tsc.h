#ifndef MENIOS_INCLUDE_KERNEL_TSC_H
#define MENIOS_INCLUDE_KERNEL_TSC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

void init_tsc();

uint64_t read_tsc(void);

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_KERNEL_TSC_H