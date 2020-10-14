#include <kernel/console.h>
#include <kernel/irq.h>
#include <kernel/keyboard.h>
#include <kernel/timer.h>
#include <arch.h>

int init_kernel() {
  // init_irq();
  // init_keyboard();
  // init_timer();

  puts("\n\nMeniOS is good to go.\n");
  putchar('%');

  // while(true){ };

  return 0xdeadc0de;
}
