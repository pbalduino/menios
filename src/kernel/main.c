#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <kernel/acpi.h>
#include <kernel/apic.h>
#include <kernel/console.h>
#include <kernel/file.h>
#include <kernel/fonts.h>
#include <kernel/framebuffer.h>
#include <kernel/gdt.h>
#include <kernel/heap.h>
#include <kernel/hw.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>
#include <kernel/mem.h>
#include <kernel/proc.h>
#include <kernel/rtc.h>
#include <kernel/serial.h>
#include <kernel/services.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <kernel/tsc.h>

void print_logo();

void boot_graphics_init() {
  fb_init();
  font_init();

  print_logo();

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

void thread_code(void* arg) {
  serial_printf("thread_code: Hello from thread %s!\n", current->name);
  char* text = (char*)arg;
  serial_printf("thread_code: Hello from thread %s!\n", text);
  printf("- Hello from thread %s!\n", text);
  ksleep(1000);
  printf("- Bye from thread %s!\n", text);
}

void show_clock(void* arg) {
  serial_line("");
  while(true){
    serial_line("");
    rtc_time_t time;
    rtc_time(&time);

    screen_pos_t pos;
    get_cursor_pos(&pos);
    gotoxy(118, 0);
    printf("%d-%d-%d %d:%d:%d UTC  \n", time.full_year, time.month, time.day, time.hours, time.minutes, time.seconds);
    ksleep(500);
    gotoxy(pos.x, pos.y);
  }
}

void _start() {
  serial_debug = true;

  file_init();

  serial_init();

  boot_graphics_init();

  gdt_init();

  idt_init();

  mem_init();

  acpi_init();

  apic_init();

  // timer_init();
  
  // init_scheduler();

  // init_services();

  // sti();

  // rtc_time_t time;
  // rtc_time(&time);

  // TODO: CPUs
  // smp_init();
  // TODO: Show hardware
  // TODO: Filesystem

  // kthread_t thread0;
  // kthread_t thread1;
  // kthread_t clock;

  // kthread_create(&thread0, "thread0", thread_code, (void*)"0");
  // kthread_create(&thread1, "thread1", thread_code, (void*)"1");
  // kthread_create(&clock, "clock", show_clock, NULL);

  // printf("- Created threads\n");

  init_hardware();

  puts("- Bye\n");
  serial_log("Bye\n");

  // ktread_join(&clock);
  
  hcf();
  // turn_off();
}
