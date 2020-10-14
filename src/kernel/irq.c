#include <kernel/console.h>
#include <kernel/interrupts.h>
#include <kernel/irq.h>
#include <arch.h>

idt_entry_t idt[256];
idtp_t* idtp;

void init_idt() {
  idtp->limit = (sizeof(idt_entry_t) * 256) - 1;
  idtp->base  = (uintptr_t)&idt;
  asm volatile("lidt (%0)" : : "a" (idtp));
}

void set_irq_handler(uint8_t irq_line, isr_t handler, uint16_t segment, uint8_t flags) {
  idt[IRQ_OFFSET + irq_line].base_lo = ((uintptr_t)handler & 0xFFFF);
  idt[IRQ_OFFSET + irq_line].base_hi = ((uintptr_t)handler >> 16) & 0xFFFF;
  idt[IRQ_OFFSET + irq_line].segment = segment;
  idt[IRQ_OFFSET + irq_line].zero = 0;
  idt[IRQ_OFFSET + irq_line].type_attr = flags;
}

void remap_irq() {
  outb(PIC1_COMM, ICW1);        // ICW1 to master
  outb(PIC2_COMM, ICW1);        // ICW1 to slave

  outb(PIC1_DATA, ICW2_MASTER); // ICW2 to master
  outb(PIC2_DATA, ICW2_MASTER); // ICW2 to slave

  outb(PIC1_DATA, ICW3_MASTER); // ICW3 to master
  outb(PIC2_DATA, ICW3_MASTER); // ICW3 to slave

  outb(PIC1_DATA, ICW4);        // ICW4 to master
  outb(PIC2_DATA, ICW4);        // ICW4 to slave

  outb(PIC2_DATA, 0xff);        // OCW1 to slave
  outb(PIC1_DATA, 0xfb);        //mask all ints but 2 in master
}

void irq_set_mask(uint8_t irq_line) {
  uint16_t port;
  uint8_t value;

  if(irq_line < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    irq_line -= 8;
  }
  value = inb(port) & ~(1 << irq_line);
  printf("  * Setting IRQ%d\n", irq_line);
  outb(port, value);
}

void irq_eoi() {
	// OCW2: rse00xxx
	//   r: rotate
	//   s: specific
	//   e: end-of-interrupt
	// xxx: specific interrupt line
	outb(PIC1_COMM, 0x20);
	outb(PIC2_COMM, 0x20);
}

void init_irq() {
  printf("* Setting interrupts\n");
  init_idt();

  remap_irq();

  asm volatile("sti");
}
