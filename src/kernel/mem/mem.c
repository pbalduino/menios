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

static uint8_t arena[PAGE_SIZE * HEAP_SIZE];

void mem_init() {
  serial_puts("\n- Initing memory management:\n");
  // init the physical memory management
  pmm_init();

  init_heap((void*)arena, PAGE_SIZE * HEAP_SIZE);
}

void mem_compactor(void*) {
  serial_line("");
  while(true) {
    serial_line("");
    ksleep(5000);
    serial_line("");
    heap_compactor();
    serial_line("");
  }
  serial_line("");
}

void init_memory_compactor() {
  serial_line("");
  kthread_p pthread = kmalloc(sizeof(kthread_t));
  serial_line("");
  kthread_create(pthread, "heap_compactor", mem_compactor, NULL);
  serial_line("");
  puts(".");
  serial_line("");
}