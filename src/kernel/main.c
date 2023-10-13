#include <stdint.h>
#include <stddef.h>
#include <kernel/console.h>
#include <kernel/fonts.h>
#include <kernel/framebuffer.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>
#include <kernel/pmap.h>
#include <kernel/smp.h>

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

  // draw_background();

  puts("Welcome to meniOS 0.0.2 64bits\n\n- Typeset test:");
  for(int c = ' '; c < 128; c++) {
    if((c - ' ') % 16 == 0) {
      puts("\n ");
    }

    putchar(c);
    putchar(' ');
  }
  putchar('\n');
  printf("- Screen mode: %lu x %lu x %d\n", fb_width(), fb_height(), fb_bpp());
  printf("- Available modes: %lu\n", fb_mode_count());
  fb_list_modes();
}

void _start() {
  // FIXME: needs to finish the typefaces
  boot_graphics_init();

  // GDT
  gdt_init();

  // IDT
  idt_init();

  // TODO: Paging
  mem_init();

  // TODO: CPUs
  smp_init();
  // TODO: APIC / LAPIC
  // TODO: Show hardware
  // TODO: Filesystem
  puts("- Bye\n");

  hcf();
}
