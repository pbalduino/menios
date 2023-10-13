#include <kernel/timer.h>
#include <kernel/console.h>

extern void reset_timer();

void timer_init() {
  printf("  Starting timer.");
  reset_timer();
  printf(" OK\n");
}
