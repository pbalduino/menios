#ifndef MENIOS_INCLUDE_KERNEL_APIC_H
#define MENIOS_INCLUDE_KERNEL_APIC_H

#include <types.h>

#define CPUID_INFO 0x1

#define LAPIC_BASE_MSR 0x1b

#define PIC1_COMMAND_PORT 0x20
#define PIC1_DATA_PORT    0x21
#define PIC2_COMMAND_PORT 0xa0
#define PIC2_DATA_PORT    0xa1

void apic_init();

#endif
