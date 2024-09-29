/**
 * mem.c - Memory Manager
 * Contains the initializing level functions for memory manager
 */
#include <kernel/heap.h>
#include <kernel/mem.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

uint8_t arena[PAGE_SIZE * HEAP_SIZE];

void mem_init() {
    serial_puts("\n- Initing memory management:\n");
    // init the physical memory management
    pmm_init();

    init_heap((void*)arena, PAGE_SIZE * HEAP_SIZE);
}