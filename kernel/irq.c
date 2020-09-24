#include <kernel/pic.h>
#include <kernel/isr.h>
#include <stdio.h>
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
  outb(PIC1_COMM, 0x20);

  isr_t handler = get_interrupt_handler(regs.int_no);
  if (handler) {
     printf("Handling int %x\n", regs.int_no);
     handler(regs);
   } else {
     printf("No handling for int %x\n", regs.int_no);
   }
}
