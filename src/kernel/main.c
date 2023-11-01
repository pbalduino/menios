#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include <kernel/acpi.h>
#include <kernel/apic.h>
// #include <kernel/console.h>
#include <kernel/fonts.h>
#include <kernel/file.h>
#include <kernel/framebuffer.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>
#include <kernel/pmap.h>
#include <kernel/serial.h>
#include <kernel/smp.h>

void boot_graphics_init() {
  fb_init();
  font_init();

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
  // fb_list_modes();
}

void _start() {
  int c;

  serial_init();
  serial_puts("\n-----  starting -----\n");

  file_init();

  // FIXME: needs to finish the typefaces
  boot_graphics_init();

  // GDT
  gdt_init();

  // IDT
  idt_init();

  // TODO: Paging
  mem_init();
  printf("addr: %p", &c);

  // TODO: CPUs
  smp_init();

  // TODO: ACPI
  acpi_init();

  // TODO: APIC / LAPIC
  apic_init();
  // TODO: Show hardware
  // TODO: Filesystem

  puts("- Bye\n");
  serial_puts("----- done -----\n");

  hcf();
}
