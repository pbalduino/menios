#ifndef MENIOS_INCLUDE_KERNEL_GDT_H
#define MENIOS_INCLUDE_KERNEL_GDT_H

#include <types.h>

#define GDT_ENTRY_COUNT 11

enum {
  GDT_ENTRY_NULL = 0,
  GDT_ENTRY_CODE16,
  GDT_ENTRY_DATA16,
  GDT_ENTRY_CODE32,
  GDT_ENTRY_DATA32,
  GDT_ENTRY_KERNEL_CODE,
  GDT_ENTRY_KERNEL_DATA,
  GDT_ENTRY_USER_CODE,
  GDT_ENTRY_USER_DATA,
  GDT_ENTRY_TSS_LOW,
  GDT_ENTRY_TSS_HIGH,
};

#define KERNEL_CODE_SEGMENT (GDT_ENTRY_KERNEL_CODE << 3)
#define KERNEL_DATA_SEGMENT (GDT_ENTRY_KERNEL_DATA << 3)
#define USER_CODE_SEGMENT   (GDT_ENTRY_USER_CODE << 3)
#define USER_DATA_SEGMENT   (GDT_ENTRY_USER_DATA << 3)
#define TSS_SEGMENT         (GDT_ENTRY_TSS_LOW << 3)

typedef struct __attribute__((packed)) {
    uint16_t size;
    uint64_t offset;
} gdt_pointer_t;

// Intel - 64 and IA-32 Architectures Software Developers Manual
// Vol 3A - pg 9-18
typedef struct __attribute__((packed)) {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t  base_middle;
  uint8_t  access;
  uint8_t  granularity;
  uint8_t  base_high;
} gdt_entry_t;

extern void gdt_load(gdt_pointer_t* gdt_descriptor);

void gdt_init();
void gdt_set_entry(int index, uint64_t base, uint32_t limit, uint8_t access, uint8_t granularity);
void tss_update_kernel_stack(uint64_t stack_top);

#endif
