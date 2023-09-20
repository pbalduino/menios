#include <kernel/kernel.h>
#include <boot/framebuffer.h>

void cli() {
  fb_puts("- Stopping interruptions.\n");
  asm("cli");
}

void sti() {
  fb_puts("- Resuming interruptions.\n");
  asm("sti");
}

// Halt and catch fire function.
void hcf(void) {
  cli();

  fb_puts("- Halting the system.\n");
  // for (;;) {
  asm("hlt");
  // }
}
