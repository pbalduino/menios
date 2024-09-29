#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <kernel/acpi.h>
#include <kernel/apic.h>
#include <kernel/file.h>
#include <kernel/fonts.h>
#include <kernel/framebuffer.h>
#include <kernel/gdt.h>
#include <kernel/heap.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>
#include <kernel/mem.h>
#include <kernel/serial.h>

void boot_graphics_init() {
  fb_init();
  font_init();

  puts("Welcome to meniOS 0.0.3 64bits\n\n- Typeset test:");
  for(int c = ' '; c < 128; c++) {
    if(c % 32 == 0) {
      puts("\n ");
    }

    putchar(c);
    putchar(' ');
  }
  putchar('\n');
  printf("- Screen mode: %lu x %lu x %d\n", fb_width(), fb_height(), fb_bpp());
  printf("  Available modes: %lu\n", fb_mode_count());
  // fb_list_modes();
}

void turn_off() {
  printf("  Preparing shutdown...\n");
  acpi_shutdown();
  printf("  OH NOES!\n");
  hcf();
}

void _start() {
  file_init();

  serial_init();

  boot_graphics_init();

  gdt_init();

  idt_init();

  mem_init();

  acpi_init();

  // TODO: CPUs
  // smp_init();
  // TODO: APIC / LAPIC
  // apic_init();
  // TODO: Show hardware
  // TODO: Filesystem

  puts("- Bye\n");
  serial_log("Bye\n");

  // char* x = (char*)0xffff800000000000;
  // serial_puts(x);

  // hcf();
  turn_off();
}
