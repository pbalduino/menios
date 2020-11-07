#include <kernel/console.h>
#include <kernel/kheap.h>
#include <types.h>

extern char __START_BSS__[];
extern char __END_BSS__[];

heap_t* heap;

void init_heap() {
  if(heap) return;

  uint32_t heap_start = (uint32_t)__START_BSS__;
  uint32_t heap_end = (uint32_t)__END_BSS__;

  printf("Initing heap.\n");
  // printf("BSS: [0x%x <-> 0x%x]\n", (uint32_t)heap_end, (uint32_t)heap_start);

  heap = (heap_t*)heap_start;
  heap->start = heap_start;
  heap->end = heap_end;
  // heap->first = 0;
  printf("Heap: 0x%x, sz: %u\n", (uint32_t)heap, sizeof(heap_t));
}

void* kmalloc(size_t size) {
  init_heap();
  printf("kmalloc'ing %u bytes\n", size);

  printf("heap_t sz: %u\n", sizeof(heap_t));

  return NULL;
}
