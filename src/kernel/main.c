/*
 * meniOS - An operating system project written from scratch for fun.
 *
 * File: main.c
 * Description: Entry file for the meniOS kernel
 *
 * Author: Plínio Balduino
 * License: MIT License
 * Copyright (c) 2020-2025 Plínio Balduino
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
#include <string.h>

#include <kernel/acpi.h>
#include <kernel/apic.h>
#include <kernel/block_device.h>
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
#include <kernel/syscall.h>
#include <kernel/user_mode.h>
#include <kernel/driver/ps2kb.h>
#include <kernel/vfs.h>

void print_logo();

static void heap_demo(void) {
  heap_stats_t before = heap_get_stats();
  logk("Heap demo: regions=%lu total=%lu used=%lu free=%lu\n",
    (unsigned long)before.region_count,
    (unsigned long)before.total_bytes,
    (unsigned long)before.used_bytes,
    (unsigned long)before.free_bytes);

  const size_t first_size = 512 * 1024;
  void* block = kmalloc(first_size);

  if(block == NULL) {
    logk("Heap demo: initial allocation failed\n");
    return;
  }

  memset(block, 0xa5, first_size);

  void* grown = krealloc(block, 1024 * 1024);
  if(grown) {
    block = grown;
  }

  void* zeroed = kcalloc(128, sizeof(uint64_t));

  heap_stats_t after = heap_get_stats();
  logk("Heap demo: post-alloc regions=%lu total=%lu used=%lu free=%lu\n",
    (unsigned long)after.region_count,
    (unsigned long)after.total_bytes,
    (unsigned long)after.used_bytes,
    (unsigned long)after.free_bytes);

  if(zeroed) {
    kfree(zeroed);
  }

  kfree(block);

  heap_stats_t final = heap_get_stats();
  logk("Heap demo: cleaned regions=%lu total=%lu used=%lu free=%lu\n",
    (unsigned long)final.region_count,
    (unsigned long)final.total_bytes,
    (unsigned long)final.used_bytes,
    (unsigned long)final.free_bytes);
}

void boot_graphics_init() {
  fb_init();
  font_init();

  print_logo();

  printf("Welcome to meniOS 0.0.4\n\n");
  logk("Testing console..OK\n");
  logk("Screen mode: %lu x %lu x %d\n", fb_width(), fb_height(), fb_bpp());
  logk("Available modes: %lu\n", fb_mode_count());
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

static void kernel_idle_loop(void) {
  for(;;) {
    __asm__("hlt");
  }
}

void _start() {
  serial_debug = true;
  tsc_init();

  serial_init();

  mem_init();

  file_system_init();

  block_device_system_init();

  vfs_init();

  boot_graphics_init();

  gdt_init();

  idt_init();

  syscall_init();

  printf("Heap demo\n");
  logk("Heap demo\n");
  heap_demo();
  logk("Finished heap demo\n");

  acpi_init();

  apic_init();

  timer_init();
  
  scheduler_init();

  hardware_init();

  user_init_launch();

  logk("Enabling interruptions\n");
  enable_interrupts();

  logk("Kernel entering idle loop\n");
  serial_log("Kernel entering idle loop\n");

  kernel_idle_loop();
}
