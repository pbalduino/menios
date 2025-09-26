#ifndef MENIOS_INCLUDE_KERNEL_ELF_LOADER_H
#define MENIOS_INCLUDE_KERNEL_ELF_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <types.h>

#include <kernel/proc.h>

bool elf64_load_image(proc_info_p proc, phys_addr_t root_phys, const void* image, size_t size, uint64_t* entry_out);

#endif
