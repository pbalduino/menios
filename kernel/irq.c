#include <kernel/pic.h>
#include <kernel/isr.h>
#include <stdio.h>
#include <types.h>
#include <x86.h>

void irq_handler(registers_t regs) {
  printf("Received IRQ");
  // Send an EOI (end of interrupt) signal to the PICs.
  // If this interrupt involved the slave.
  if (regs.int_no >= 40) {
    outb(PIC2_COMM, 0x20);
  } else {
    printf("no no no\n");
  }
  // Send reset signal to master. (As well as slave, if necessary).
  irq_eoi();

  isr_t handler = get_interrupt_handler(regs.int_no);
  if (handler) {
    printf("Handling int %x\n", regs.int_no);
    handler(regs);
  } else {
    printf("No handling for int %x\n", regs.int_no);
  }
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
  value = inb(port) | (1 << irq_line);
  printf("Setting IRQ %d with %x\n", irq_line, value);
  outb(port, value);
}

void irq_clear_mask(uint8_t irq_line) {
  uint16_t port;
  uint8_t value;

  if(irq_line < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    irq_line -= 8;
  }
  value = inb(port) & ~(1 << irq_line);
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
