#include <stdint.h>
#include <stddef.h>
#include <boot/fonts.h>
#include <boot/framebuffer.h>
#include <kernel/kernel.h>
#include <kernel/gdt.h>

void draw_background() {
  int lighter = FB_DARK_BLUE;

  for (uint64_t x = 0; x < fb_width(); x++) {
    for (uint64_t y = 0; y < fb_height(); y++) {
      fb_putpixel(x, y, lighter - ((lighter * y) / fb_height()));
    }
  }
}

void boot_graphics_init() {
  fb_init();
  font_init();

  draw_background();

  fb_puts("Welcome to meniOS 0.0.2 64bits\n\n- Typeset test:");

  for(int c = ' '; c < 128; c++) {
    if((c - ' ') % 16 == 0) {
      fb_puts("\n ");
    }

    fb_putchar(c);
    fb_putchar(' ');
  }
  fb_puts("\n");
}

void _start() {
  boot_graphics_init();

  // GDT
  gdt_init();

  // TSS

  // IDT
  // CPUs
  // memory
  // filesystem
  // what else?

  hcf();
}
