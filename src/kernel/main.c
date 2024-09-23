#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <kernel/acpi.h>
#include <kernel/apic.h>
#include <kernel/fonts.h>
#include <kernel/file.h>
#include <kernel/framebuffer.h>
#include <kernel/gdt.h>
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

  typedef struct dummy {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
  } dummy_t;

  dummy_t* dummy1 = malloc(sizeof(dummy_t));
  if(dummy1 == NULL) {
    printf("= ERROR: Failed to allocate memory =\n");
    hcf();
  }
/*
  dummy1->a = 0x42;
  dummy1->b = 0x69;
  dummy1->c = 0x86;
  dummy1->d = 0xa5;
  
  serial_printf("- Dummy 1: %x %x %x %x @ %p\n", dummy1->a, dummy1->b, dummy1->c, dummy1->d, dummy1);

  dummy_t* dummy2 = malloc(sizeof(dummy_t));
  if(dummy2 == NULL) {
    printf("= ERROR: Failed to allocate memory =\n");
    hcf();
  }

  dummy2->a = 0x10;
  dummy2->b = 0x20;
  dummy2->c = 0x30;
  dummy2->d = 0x40;
  serial_printf("- Dummy 2: %x %x %x %x @ %p\n", dummy2->a, dummy2->b, dummy2->c, dummy2->d, dummy2);
*/
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
