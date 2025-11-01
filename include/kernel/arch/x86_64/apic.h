#ifndef MENIOS_INCLUDE_KERNEL_ARCH_X86_64_APIC_H
#define MENIOS_INCLUDE_KERNEL_ARCH_X86_64_APIC_H

#include <stdbool.h>
#include <types.h>

#define CPUID_INFO 0x1

#define LAPIC_BASE_MSR 0x1b
#define LAPIC_BASE_X2APIC_ENABLE (1ull << 10)

#define IA32_X2APIC_BASE 0x00000800u
#define IA32_X2APIC_APICID 0x00000802u

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

void apic_initialize(void);
void lapic_timer_initialize(void);
void timer_frequency(uint32_t freq);
void write_lapic(uint32_t reg, uint32_t value);

uint32_t read_lapic(uint32_t reg);

bool apic_configure_irq(uint32_t gsi,
                        uint8_t vector,
                        bool level_triggered,
                        bool active_low);

bool apic_update_irq_mask(uint32_t gsi, bool masked);

void apic_send_eoi(void);
uint32_t apic_current_processor_id(void);
void* apic_get_lapic_base(void);
bool apic_is_x2apic_enabled(void);

#endif /* MENIOS_INCLUDE_KERNEL_ARCH_X86_64_APIC_H */
