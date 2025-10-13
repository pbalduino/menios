#ifndef MENIOS_INCLUDE_KERNEL_SYSCALL_ENTRY_H
#define MENIOS_INCLUDE_KERNEL_SYSCALL_ENTRY_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

void syscall_arch_init(void);
void syscall_set_kernel_stack(uint64_t rsp);

#ifdef __cplusplus
}
#endif

#endif
