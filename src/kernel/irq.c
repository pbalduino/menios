#include <kernel/console.h>
#include <kernel/interrupts.h>
#include <kernel/irq.h>
#include <arch.h>

void remap_irq() {
  printf("* Remapping IRQs\n");
  outb(PIC1_COMM, ICW1);        // ICW1 to master
  outb(PIC2_COMM, ICW1);        // ICW1 to slave

  outb(PIC1_DATA, ICW2_MASTER); // ICW2 to master
  outb(PIC2_DATA, ICW2_MASTER); // ICW2 to slave

  outb(PIC1_DATA, ICW3_MASTER); // ICW3 to master
  outb(PIC2_DATA, ICW3_MASTER); // ICW3 to slave

  outb(PIC1_DATA, ICW4);        // ICW4 to master
  outb(PIC2_DATA, ICW4);        // ICW4 to slave

  outb(PIC2_DATA, 0xff);        // OCW1 to slave
  outb(PIC1_DATA, 0xff);        //mask all ints but 2 in master
}

void set_irq_handler(uint8_t irq_line, isr_t handler, uint16_t segment, uint8_t flags) {
  set_int_handler(IRQ_OFFSET + irq_line, handler, segment, flags);
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
  printf("* Enabling IRQ%d\n", irq_line);
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
  remap_irq();
}
