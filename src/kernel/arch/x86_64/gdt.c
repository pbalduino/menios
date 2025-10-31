#include <kernel/console.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/serial.h>
#include <stdio.h>
#include <string.h>

static gdt_entry_t gdt[GDT_ENTRY_COUNT];
static gdt_pointer_t gdt_p;

typedef struct __attribute__((packed, aligned(8))) {
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t io_map_base;
} tss_t;

typedef struct __attribute__((packed)) {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t  base_middle;
  uint8_t  access;
  uint8_t  granularity;
  uint8_t  base_high;
  uint32_t base_upper;
  uint32_t reserved;
} gdt_tss_entry_t;

#define TSS_STACK_SIZE 0x4000

static tss_t tss __attribute__((aligned(16)));
static uint8_t tss_stack[TSS_STACK_SIZE] __attribute__((aligned(16)));

void gdt_set_entry(int index, uint64_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
  gdt_entry_t* entry = &gdt[index];

  entry->limit_low = limit & 0xffff;
  entry->base_low = base & 0xffff;
  entry->base_middle = (base >> 16) & 0xff;
  entry->access = access;
  entry->granularity = ((limit >> 16) & 0x0f) | (granularity & 0xf0);
  entry->base_high = (base >> 24) & 0xff;
}

static void gdt_set_tss(int index, uint64_t base, uint32_t limit) {
  gdt_tss_entry_t* entry = (gdt_tss_entry_t*)&gdt[index];
  memset(entry, 0, sizeof(gdt_tss_entry_t));

  entry->limit_low = limit & 0xffff;
  entry->base_low = base & 0xffff;
  entry->base_middle = (base >> 16) & 0xff;
  entry->access = 0x89; // Present, type 9 (available 64-bit TSS)
  entry->granularity = ((limit >> 16) & 0x0f);
  entry->base_high = (base >> 24) & 0xff;
  entry->base_upper = (uint32_t)(base >> 32);
  entry->reserved = 0;
}

static void gdt_initialize_tss(void) {
  memset(&tss, 0, sizeof(tss));
  memset(tss_stack, 0, sizeof(tss_stack));

  tss.rsp0 = (uint64_t)(tss_stack + sizeof(tss_stack));
  tss.io_map_base = sizeof(tss);

  gdt_set_tss(GDT_ENTRY_TSS_LOW, (uint64_t)&tss, sizeof(tss) - 1);
}

void gdt_initialize(void) {
  serial_log("Entering gdt_initialize");
  logk("Setting GDT");

  gdt_set_entry(GDT_ENTRY_NULL, 0, 0, 0, 0);
  gdt_set_entry(GDT_ENTRY_CODE16, 0, 0xffff, 0x9a, 0x80);
  gdt_set_entry(GDT_ENTRY_DATA16, 0, 0xffff, 0x92, 0x80);
  gdt_set_entry(GDT_ENTRY_CODE32, 0, 0x000fffff, 0x9a, 0xcf);
  gdt_set_entry(GDT_ENTRY_DATA32, 0, 0x000fffff, 0x92, 0xcf);
  gdt_set_entry(GDT_ENTRY_KERNEL_CODE, 0, 0, 0x9a, 0x20);
  gdt_set_entry(GDT_ENTRY_KERNEL_DATA, 0, 0, 0x92, 0x00);
  gdt_set_entry(GDT_ENTRY_USER_CODE, 0, 0, 0xfa, 0x20);
  gdt_set_entry(GDT_ENTRY_USER_DATA, 0, 0x000fffff, 0xf2, 0xcf);
  gdt_initialize_tss();

  // Create a GDT pointer
  gdt_p.size = sizeof(gdt) - 1;
  gdt_p.offset = (uint64_t)&gdt;

  puts("...");

  gdt_load(&gdt_p);

  uint16_t tss_selector = TSS_SEGMENT;
  asm volatile("ltr %w0" : : "r" (tss_selector) : "memory");

  puts("OK\n");
  serial_log("Leaving gdt_initialize");
}

void tss_update_kernel_stack(uint64_t stack_top) {
  tss.rsp0 = stack_top;
}

uint64_t gdt_get_tss_stack_top(void) {
  return tss.rsp0;
}
