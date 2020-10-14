#include <kernel/console.h>
#include <kernel/interrupts.h>
#include <kernel/irq.h>
#include <kernel/timer.h>

extern void irq_timer();
extern void reset_timer();

void timer_handler(registers_t* regs) {
  if(regs){ };

  putchar('.');

  irq_eoi();
}

void init_timer() {
  printf("Initing timer\n");

  reset_timer();
  
  set_irq_handler(IRQ_TIMER, irq_timer, GD_KT, IDT_PRESENT | IDT_32BIT_INTERRUPT_GATE);

  irq_set_mask(IRQ_TIMER);
}
