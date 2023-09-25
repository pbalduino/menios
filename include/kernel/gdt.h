#ifndef MENIOS_INCLUDE_KERNEL_GDT_H
#define MENIOS_INCLUDE_KERNEL_GDT_H

#include <types.h>

#define KERNEL_CODE_SEGMENT 0x28
#define KERNEL_DATA_SEGMENT 0x30

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

typedef struct __attribute__((packed)) {
  gdt_entry_t null;
  gdt_entry_t _16bit_code;
  gdt_entry_t _16bit_data;
  gdt_entry_t _32bit_code;
  gdt_entry_t _32bit_data;
  gdt_entry_t _64bit_code;
  gdt_entry_t _64bit_data;
  gdt_entry_t user_data;
  gdt_entry_t user_code;
} gdt_t;

extern void gdt_load(gdt_pointer_t* gdt_descriptor);

void gdt_init();
void gdt_set_entry(int index, uint64_t base, uint32_t limit, uint8_t access, uint8_t granularity);

#endif
