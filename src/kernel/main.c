/*
 * meniOS - An operating system project written from scratch for fun.
 *
 * File: main.c
 * Description: Entry file for the meniOS kernel
 *
 * Author: Plínio Balduino
 * License: MIT License
 * Copyright (c) 2020-2024 Plínio Balduino
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the “Software”), to 
 * deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

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

  printf("Welcome to meniOS 0.0.3\n\n");
  logk("Testing console..OK\n");
  logk("Screen mode: %lu x %lu x %d\n", fb_width(), fb_height(), fb_bpp());
  logk("Available modes: %lu\n", fb_mode_count());
  // fb_list_modes();
}

void turn_off() {
  printf("  Preparing shutdown...\n");
  acpi_shutdown();
  printf("  OH NOES!\n");
  halt();
}

void thread_code(void* arg) {
  serial_printf("thread_code: Hello from thread %s!\n", current->name);
  char* text = (char*)arg;
  serial_printf("thread_code: Hello from thread %s!\n", text);
  logk("Hello from thread %s!\n", text);
  ksleep(1000);
  logk("Bye from thread %s!\n", text);
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
    printf("%4d-%02d-%02d %02d:%02d:%02d UTC  \n", time.full_year, time.month, time.day, time.hours, time.minutes, time.seconds);
    ksleep(500);
    gotoxy(pos.x, pos.y);
  }
}

int show_caret() {
  while(true){
    puts("_");
    ksleep(500);
    puts("\b");
    ksleep(500);
  }
  return 0;
}

void _start() {
  serial_debug = true;
  tsc_init();

  file_init();

  serial_init();

  boot_graphics_init();

  gdt_init();

  idt_init();

  mem_init();

  acpi_init();

  apic_init();

  timer_init();
  
  scheduler_init();

  // init_services();

  // enable_interrupts();

  // rtc_time_t time;
  // rtc_time(&time);

  // TODO: CPUs
  // smp_init();
  // TODO: Show hardware
  // TODO: Filesystem

  // kthread_t thread0;
  // kthread_t thread1;
  // kthread_t clock;
  kthread_t caret;

  // kthread_create(&thread0, "thread0", thread_code, (void*)"0");
  // kthread_create(&thread1, "thread1", thread_code, (void*)"1");

  hardware_init();

  // logk("Sleeping for five seconds\n");
  // ksleep(5000);
  // logk("Back\n");

  // kthread_create(&clock, "clock", show_clock, NULL);
  // logk("Created clock thread\n");

  logk("Enabling interruptions\n");
  enable_interrupts();

  printf("menios# ");

  kthread_create(&caret, "caret", show_caret, NULL);

  while(true){
    // int ch = getchar();
    // putchar(ch);
  }

  logk("Bye\n");
  serial_log("Bye\n");

  // ktread_join(&clock);
  
  halt();
  // turn_off();
}
