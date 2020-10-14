#include <kernel/console.h>
#include <kernel/irq.h>
#include <kernel/keyboard.h>
#include <kernel/timer.h>
#include <arch.h>
#include <assert.h>

int init_kernel() {
  init_irq();

  init_keyboard();
  init_timer();

  puts("* MeniOS is good to go.\n\n# ");

  uint8_t c;
  while(true) {
    c = getchar();
    if(c == 27) {
      break;
    } else if(c) {
      putchar(c);
    }
  };

  puts("\n* Bye!\n");

  return 0xdeadc0de;
}
