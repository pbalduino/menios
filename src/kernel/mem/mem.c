/**
 * mem.c - Memory Manager
 * Contains the initializing level functions for memory manager
 */
#include <kernel/mem.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

void mem_init() {
    serial_puts("\n- Initing memory management:\n");
    // init the physical memory management
    pmm_init();
}