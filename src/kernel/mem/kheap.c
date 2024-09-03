#include <kernel/kheap.h>
#include <kernel/mem.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <mem.h>
#include <stddef.h>
#include <types.h>

static uint8_t arena[PAGE_SIZE * KHEAP_SIZE];
struct kheap_node* kheap;

void init_arena() {
  serial_printf("kmalloc: First time kmallocin'\n");
  kheap = (struct kheap_node*)arena;
  memcpy(kheap->magic, KHEAP_MAGIC, sizeof(kheap->magic));
  kheap->next = NULL;
  kheap->size = (PAGE_SIZE * KHEAP_SIZE) - KHEAP_HEADER;
  kheap->status = KHEAP_FREE;
}

void* kmalloc(uint32_t size) {
  if(kheap == NULL) {
    init_arena();
  }

  serial_printf("kmalloc: arena @ %p\n", kheap);

  // iterate through kheap until find a free node that fits size
  // save the address
  // save .size as old_size
  // mark the node as used
  // update the .size with size value
  // move .data + size and add a new free node with .size = old_size - (size + KHEAP_HEADER)
}