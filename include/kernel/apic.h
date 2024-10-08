#ifndef MENIOS_INCLUDE_KERNEL_APIC_H
#define MENIOS_INCLUDE_KERNEL_APIC_H

#include <types.h>

#define CPUID_INFO 0x1

#define LAPIC_BASE_MSR 0x1b

#define PIC1_COMMAND_PORT 0x20
#define PIC1_DATA_PORT    0x21
#define PIC2_COMMAND_PORT 0xa0
#define PIC2_DATA_PORT    0xa1

#define IOAPIC_REG_ENTRYCOUNT 1

#define LAPIC_SVR           0x0f0
#define LAPIC_EOI           0x0b0
#define LAPIC_TIMER_DIV     0x3e0
#define LAPIC_TIMER_INIT    0x380
#define LAPIC_TIMER_CURR    0x390
#define LAPIC_TIMER_LVT     0x320

#define DEFAULT_LAPIC_ADDRESS 0xfee00000

#define DIV_BY_2    0x00
#define DIV_BY_4    0x01
#define DIV_BY_8    0x02
#define DIV_BY_16   0x03
#define DIV_BY_32   0x08
#define DIV_BY_64   0x09
#define DIV_BY_128  0x0a
#define DIV_BY_1    0x0b

void apic_init();
void lapic_timer_init();
void timer_frequency(uint32_t freq);
void write_lapic(uintptr_t reg, uint32_t value);

uint32_t read_lapic(uintptr_t reg);

#endif
