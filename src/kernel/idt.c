#include <kernel/console.h>
#include <kernel/interrupts.h>

idt_entry_t idt[256];
idtp_t* idtp;

void init_idt() {
  printf("* Setting interrupt descriptor table\n");
  idtp->limit = (sizeof(idt_entry_t) * 256) - 1;
  idtp->base  = (uintptr_t)&idt;

  #ifdef DEBUG
  printf("  IDT located at 0x%x\n", idtp->base);
  printf("  IDTP located at 0x%x\n", (uintptr_t)&idtp);
  #endif

  asm volatile("lidt (%0)" : : "a" (idtp));
}

void set_int_handler(uint8_t int_line, isr_t handler, uint16_t segment, uint8_t flags) {
  printf("* Setting handler for interrupt %d\n", int_line);
  idt[int_line].base_lo = ((uintptr_t)handler & 0xFFFF);
  idt[int_line].base_hi = ((uintptr_t)handler >> 16) & 0xFFFF;
  idt[int_line].segment = segment;
  idt[int_line].zero = 0;
  idt[int_line].type_attr = flags;
  #ifdef DEBUG
  printf("  Lo: 0x%x - Hi: 0x%x - Segment: 0x%x - Attr: 0x%x\n",
    idt[int_line].base_lo,
    idt[int_line].base_hi,
    idt[int_line].segment,
    idt[int_line].type_attr);
  #endif
}
