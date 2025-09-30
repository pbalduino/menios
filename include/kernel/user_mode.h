#ifndef MENIOS_INCLUDE_KERNEL_USER_MODE_H
#define MENIOS_INCLUDE_KERNEL_USER_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

void user_mode_enter(void (*entrypoint)(void*), void* user_stack_top, uint64_t arg);
void user_init_launch(void);
void user_mosh_launch(void);
void user_demo_launch(void);

#ifdef __cplusplus
}
#endif

#endif
