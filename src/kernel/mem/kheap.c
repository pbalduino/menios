#include <kernel/heap.h>
#include <kernel/mem.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <mem.h>
#include <stddef.h>
#include <types.h>

static uint8_t arena[PAGE_SIZE * HEAP_SIZE];
heap_node_p heap;

void init_arena() {
  serial_printf("kmalloc: First time kmallocin'\n");
  heap = (heap_node_p)arena;
  memcpy(heap->magic, HEAP_MAGIC, sizeof(heap->magic));
  heap->next = NULL;
  heap->size = (PAGE_SIZE * HEAP_SIZE) - HEAP_HEADER;
  heap->status = HEAP_FREE;
}

void* kmalloc(uint32_t size) {
  if(heap == NULL) {
    init_arena();
  }

  serial_printf("kmalloc: arena @ %p\n", heap);

  // iterate through kheap until find a free node that fits size
  // save the address
  // save .size as old_size
  // mark the node as used
  // update the .size with size value
  // move .data + size and add a new free node with .size = old_size - (size + KHEAP_HEADER)
}