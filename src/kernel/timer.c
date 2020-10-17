#include <kernel/console.h>
#include <kernel/interrupts.h>
#include <kernel/irq.h>
#include <kernel/timer.h>

#define PARTS 8
#define DELAY 20

extern void irq_timer();
extern void reset_timer();

static uint8_t spin_pos;

void timer_handler(registers_t* regs) {
  if(regs) {};

  char* spinner[PARTS] = {"[=   ]",
                          "[==  ]",
                          "[=== ]",
                          "[====]",
                          "[ ===]",
                          "[  ==]",
                          "[   =]",
                          "[    ]"};
  uint16_t old_pos = get_cursor_position();

  set_cursor_position(0x4a);
  puts(spinner[spin_pos / DELAY]);
  set_cursor_position(old_pos);

  ++spin_pos;
  spin_pos %= PARTS * DELAY;

  irq_eoi();
}

void init_timer() {
  printf("* Initing timer\n");

  reset_timer();

  set_irq_handler(IRQ_TIMER, irq_timer, GD_KT, IDT_PRESENT | IDT_32BIT_INTERRUPT_GATE);

  irq_set_mask(IRQ_TIMER);
}
