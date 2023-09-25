#include <kernel/console.h>
#include <kernel/kernel.h>

void cli() {
  puts("- Stopping interruptions.\n");
  asm("cli");
}

void sti() {
  puts("- Resuming interruptions.\n");
  asm("sti");
}

// Halt and catch fire function.
void hcf(void) {
  cli();

  puts("- Halting the system.\n");
  // for (;;) {
  asm("hlt");
  // }
}
