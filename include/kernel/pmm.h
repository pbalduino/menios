#ifndef MENIOS_INCLUDE_KERNEL_PMM_H
#define MENIOS_INCLUDE_KERNEL_PMM_H

#include <stdint.h>

uint64_t read_cr3();
void pmm_init();
void write_cr3(uint64_t value);

#endif
