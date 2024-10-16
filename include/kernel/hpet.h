#ifndef MENIOS_INCLUDE_KERNEL_HPET_H
#define MENIOS_INCLUDE_KERNEL_HPET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#include <kernel/acpi.h>

#define HPET_BASE_ADDRESS 0xF1000000
#define HPET_REG_CAPABILITIES 0x00
#define HPET_REG_CONFIGURATION 0x10
#define HPET_REG_INTERRUPT_STATUS 0x20
#define HPET_REG_MAIN_COUNTER 0xF0
#define HPET_REG_TIMER0_CONFIG 0x100
#define HPET_REG_TIMER0_COMPARATOR 0x108
#define HPET_REG_TIMER0_COUNTER 0x110
#define HPET_REG_TIMER1_CONFIG 0x120
#define HPET_REG_TIMER1_COMPARATOR 0x128
#define HPET_REG_TIMER1_COUNTER 0x130
#define HPET_REG_TIMER2_CONFIG 0x140
#define HPET_REG_TIMER2_COMPARATOR 0x148
#define HPET_REG_TIMER2_COUNTER 0x150
#define HPET_REG_TIMER3_CONFIG 0x160
#define HPET_REG_TIMER3_COMPARATOR 0x168
#define HPET_REG_TIMER3_COUNTER 0x170
#define HPET_REG_TIMER4_CONFIG 0x180
#define HPET_REG_TIMER4_COMPARATOR 0x188
#define HPET_REG_TIMER4_COUNTER 0x190
#define HPET_REG_TIMER5_CONFIG 0x1A0
#define HPET_REG_TIMER5_COMPARATOR 0x1A8
#define HPET_REG_TIMER5_COUNTER 0x1B0

#define HPET_OK     0
#define HPET_ERROR -1

#define HPET_GCR_OFFSET 0x10

typedef int hpet_status_t;

typedef struct hpet_table_t {
  acpi_sdt_header_t header;  // Standard ACPI header
  uint32_t          event_timer_block_id;
  acpi_address_t    address;  // Contains the base address of HPET
  uint8_t           hpet_number;
  uint16_t          minimum_tick;
  uint8_t           page_protection;
} __attribute__((packed)) hpet_table_t;

hpet_status_t hpet_timer_init();

#ifdef __cplusplus
}
#endif

#endif //MENIOS_INCLUDE_KERNEL_HPET_H