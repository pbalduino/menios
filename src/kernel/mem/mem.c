/**
 * mem.c - Memory Manager
 * Contains the initializing level functions for memory manager
 */
#include <kernel/heap.h>
#include <kernel/mem.h>
#include <kernel/pmm.h>
#include <kernel/thread.h>
#include <kernel/serial.h>
#include <stdio.h>
#include <string.h>

void mem_init() {
  serial_puts("\n- Initing memory management:\n");
  // init the physical memory management
  pmm_init();

  heap_init(NULL, PAGE_SIZE * HEAP_SIZE);
}

int mem_compactor(void *unused) {
  serial_line("");
  while(true) {
    serial_line("");
    ksleep(5000);
    serial_line("");
    heap_compactor();
    serial_line("");
  }
  serial_line("");
  (void)unused;
  return 0;
}

void mem_compactor_init() {
  kthread_p pthread = kmalloc(sizeof(kthread_t));
  if(pthread == NULL) {
    serial_error("mem_compactor_init: failed to allocate thread descriptor\n");
    return;
  }
  memzero(pthread, sizeof(kthread_t));
  kthread_create(pthread, "heap_compactor", mem_compactor, NULL);
  puts(".");
}
