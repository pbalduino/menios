#ifndef MENIOS_INCLUDE_KERNEL_GDT_H
#define MENIOS_INCLUDE_KERNEL_GDT_H

#include <kernel/types.h>

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
  uint16_t length;
  uint16_t base_low16;
  uint8_t base_mid8;
  uint8_t flags0;
  uint8_t flags1;
  uint8_t base_high8;
  uint32_t base_upper32;
  uint32_t reserved;
} tss_entry_t;

typedef struct __attribute__((packed)) {
  uint32_t reserved0;
  uint64_t rsp[3];
  uint64_t reserved1;
  uint64_t ist[7];
  uint32_t reserved2;
  uint32_t reserved3;
  uint16_t reserved4;
  uint16_t iopb_offset;
} tss_t;

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
  tss_entry_t tss;
} gdt_t;

extern void gdt_load(gdt_pointer_t* gdt_descriptor);
extern void tss_load(tss_t* tss);

void gdt_init();
void gdt_set_entry(int index, uint64_t base, uint32_t limit, uint8_t access, uint8_t granularity);

#endif
