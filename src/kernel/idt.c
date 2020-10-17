#include <kernel/console.h>
#include <kernel/interrupts.h>

idt_entry_t idt[256];
idtp_t* idtp;

void init_idt() {
  printf("* Setting interrupt descriptor table\n");
  idtp->limit = (sizeof(idt_entry_t) * 256) - 1;
  idtp->base  = (uintptr_t)&idt;

  printf("  IDT located at 0x%x\n", idtp->base);
  printf("  IDTP located at 0x%x\n", (uintptr_t)&idtp);

  asm volatile("lidt (%0)" : : "a" (idtp));
}

void set_int_handler(uint8_t int_line, isr_t handler, uint16_t segment, uint8_t flags) {
  printf("* Setting handler for interrupt %d\n", int_line);
  idt[int_line].base_lo = ((uintptr_t)handler & 0xFFFF);
  idt[int_line].base_hi = ((uintptr_t)handler >> 16) & 0xFFFF;
  idt[int_line].segment = segment;
  idt[int_line].zero = 0;
  idt[int_line].type_attr = flags;
}
