#include <kernel/console.h>
#include <kernel/isr.h>
#include <kernel/interrupts.h>
#include <arch.h>

extern void isr8();

void isr_handler(registers_t* regs) {
  printf("Handling int 0x%x with error 0x%x\n", regs->int_no, regs->err_code);
}

void init_isr() {
  printf("* Setting interrupts\n");
  set_int_handler(ISR_DOUBLE_FAULT, isr8, GD_KT, IDT_PRESENT | IDT_32BIT_TRAP_GATE);
}
