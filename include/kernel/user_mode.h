#ifndef MENIOS_INCLUDE_KERNEL_USER_MODE_H
#define MENIOS_INCLUDE_KERNEL_USER_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

void user_mode_enter(void (*entrypoint)(void*), void* user_stack_top, uint64_t arg);

#ifdef __cplusplus
}
#endif

#endif
