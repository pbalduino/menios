#ifndef MENIOS_INCLUDE_UNISTD_H
#define MENIOS_INCLUDE_UNISTD_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

int brk(void *addr);

void *sbrk(intptr_t increment);

#ifdef __cplusplus
}
#endif

#endif