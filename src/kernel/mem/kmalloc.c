#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <types.h>
#include <unity.h>

static bool heap_freed = false;
static heap_node_p heap;
static kmutex_t heap_mutex;
static size_t free_mem;

void dump_heap(heap_node_p heap, size_t size) {
  serial_printf("dump_heap: %p\n", heap);
  for(size_t i = 0; i < size; i++) {

    if(i % 16 == 0) {
      if(i != 0) {
        serial_puts("   ");
        for(size_t j = i - 15; j <= i; j++) {
          uint8_t byte = ((uint8_t*)heap)[j];
          if(byte >= 32 && byte <= 126) {
            serial_putchar(byte);
          } else {
            serial_puts(".");
          }
        }
      }

      serial_printf("\n%lx: ", ((uintptr_t)heap) + i);
    }
    
    uint8_t* byte = (uint8_t*)heap + i;
    if(*byte < 16) {
      serial_printf("0%x ", *byte);
    } else {
      serial_printf("%x ", *byte);
    }
  }
  serial_printf("\n");

}

void debug_heap(heap_node_p heap) {
#ifndef MENIOS_NO_DEBUG
  cli();
  serial_printf("debug_heap: %p\n", heap);
  while(heap) {
    serial_printf("heap @ %p - ", heap);
    serial_printf("heap->magic: %s(%lx) - ", heap->magic == HEAP_MAGIC ? "OK " : "BAD", heap->magic);
    serial_printf("heap->status: %s(%d) - ", heap->status == HEAP_FREE ? "FREE" : "USED", heap->status);
    serial_printf("heap->data: %p - ", heap->data);
    serial_printf("heap->next: %p - ", heap->next ? heap->next : NULL);
    serial_printf("heap->size: %u\n", heap->size);

    heap = (heap_node_p)heap->next;
  }
  sti();
#endif
}

void init_heap(void* addr, size_t size) {
  heap = (heap_node_p)addr;
  heap->magic = HEAP_MAGIC;
  heap->size = size - HEAP_HEADER_SIZE;
  heap->next = NULL;
  heap->status = HEAP_FREE;
  free_mem = heap->size;

  serial_printf("Heap initialized at %p with size %d\n", addr, size);
}

HEAP_INSPECT_RESULT inspect_heap(uint32_t node_index, heap_node_p* node) {
  *node = (heap_node_p)heap;
  uint32_t i = 0;

  while(i < node_index && *node && (*node)->magic == HEAP_MAGIC) {
    (*node) = (*node)->next;
    i++;
  }

  if(!(*node)) {
    printf("Node not found\n");
    return HEAP_INSPECT_INVALID_INDEX;
  }

  if((*node)->magic != HEAP_MAGIC) {
    printf("Corrupted node\n");
    return HEAP_INSPECT_CORRUPTED;
  }

  return HEAP_INSPECT_OK;
}

static int find_first_free_node(size_t size, heap_node_p* node) {
  *node = (heap_node_p)heap;

  while(*node) {
    if((*node)->status > 1) {
      serial_printf("find_first_free_node: invalid node status: %d\n", (*node)->status);
      cli();
      debug_heap(heap);
      dump_heap(heap, 0x1000);
      hcf();
    }
    if((*node)->status == HEAP_FREE && (*node)->size > size + HEAP_HEADER_SIZE) {
      return 0;
    }
    *node = (*node)->next;
  }
  if(!*node) {
    serial_printf("find_first_free_node: no free node found\n");
  }
  return -1;
}

/**
 * Returns null if size == 0 or if there's no memory
 */
void* kmalloc(size_t size) {
  if(size == 0) {
    serial_printf("kmalloc: size is 0\n");
    return NULL;
  }

  heap_node_p node = NULL;
  heap_node_p next = NULL;

  kmutex_lock(&heap_mutex);

  if(find_first_free_node(size + HEAP_HEADER_SIZE, &node) != 0) {
    serial_printf("kmalloc: no free node found. free: %d - needed: %d\n", free_mem, size);
    printf("\n-- OUT OF MEMORY --\n");
    current->errno = ENOMEM;
    debug_heap(heap);

    kmutex_unlock(&heap_mutex);

    hcf();
    return NULL;
  }

  next = (heap_node_p)(((uintptr_t)node) + HEAP_HEADER_SIZE + size);

  next->magic = HEAP_MAGIC;
  next->size = node->size - (size + HEAP_HEADER_SIZE);
  next->next = node->next;
  next->status = HEAP_FREE;
  free_mem -= (size + HEAP_HEADER_SIZE);
  node->status = HEAP_USED;
  node->size = size;
  node->next = next;

  kmutex_unlock(&heap_mutex);

  if(next->size == 0) {
    serial_printf("kmalloc: next->size is 0\n");
    debug_heap(heap);
    hcf();
  }

  return (void*)node->data;
}

void kfree(void* ptr) {
  // serial_printf("kfree: %p\n", ptr);
  if (ptr == NULL) {
    return;
  }
  kmutex_lock(&heap_mutex);

  heap_node_p node = (heap_node_p)((uintptr_t)ptr - HEAP_HEADER_SIZE);
  // serial_printf("kfree: node @ %p, free_mem: %d\n", node, free_mem);
  node->status = HEAP_FREE;
  // free_mem += node->size;
  heap_freed = true;
  kmutex_unlock(&heap_mutex);
}

void heap_compactor() {
  serial_printf("heap_compactor: initing\n");
  kmutex_lock(&heap_mutex);
  if(!heap_freed) {
    kmutex_unlock(&heap_mutex);
    serial_printf("heap_compactor: nothing to do here.\n");
    return;
  }
  heap_node_p node = heap;
  while(node) {
    if(node->status == HEAP_FREE) {
      heap_node_p next = node->next;
      while(next && next->status == HEAP_FREE) {
        node->size += next->size + HEAP_HEADER_SIZE;
        node->next = next->next;
        next = node->next;
      }
      memzero(node->data, node->size);
      serial_printf("heap_compactor: Compacted @ %p.\n", node);
    }
    node = node->next;
  }
  heap_freed = false;
  kmutex_unlock(&heap_mutex);
  serial_printf("heap_compactor: leaving.\n");
}