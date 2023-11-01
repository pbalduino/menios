#include <kernel/timer.h>
#include <stdio.h>

extern void reset_timer();

void timer_init() {
  printf("  Starting timer.");
  reset_timer();
  printf(" OK\n");
}
