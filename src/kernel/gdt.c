#include <kernel/gdt.h>
#include <kernel/console.h>

gdt_t gdt = {
  {0, 0, 0, 0, 0, 0}, // null
  {0xffff, 0, 0, 0x9a, 0x80, 0},    // 16bits code - 0x08
  {0xffff, 0, 0, 0x92, 0x80, 0},    // 16bits data - 0x10
  {0xffff, 0, 0, 0x9a, 0xcf, 0},    // 32bits code - 0x18
  {0xffff, 0, 0, 0x92, 0xcf, 0},    // 32bits data - 0x20
  {0, 0, 0, 0x9a, 0xa2, 0},         // 64bits kernel code - 0x28
  {0, 0, 0, 0x92, 0xa0, 0},         // 64bits kernel data - 0x30
  {0, 0, 0, 0xF2, 0, 0},            // 64bits user code - 0x38
  {0, 0, 0, 0xFA, 0x20, 0}         // 64bits user data - 0x40
};

gdt_pointer_t gdt_p;

void gdt_init() {
  puts("- Setting GDT");

  // Create a GDT pointer
  gdt_p.size = sizeof(gdt) - 1;
  gdt_p.offset = (uint64_t)&gdt;

  puts("...");
  // Load the GDT using inline assembly
  gdt_load(&gdt_p);

  puts(" OK\n");
}
