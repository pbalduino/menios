#include <types.h>
#include <kernel/heap.h>
#include <kernel/serial.h>
#include <stdio.h>
#include <unity.h>

static heap_node_p heap;

void debug_heap(heap_node_p heap) {
  printf("heap debug: %p\n", heap);
  while(heap) {
    serial_printf("heap @ %p - ", heap);
    serial_printf("heap->magic: %s(%lx) - ", heap->magic == HEAP_MAGIC ? "OK " : "BAD", heap->magic);
    serial_printf("heap->status: %s(%d) - ", heap->status == HEAP_FREE ? "FREE" : "USED", heap->status);
    serial_printf("heap->data: %p - ", heap->data);
    serial_printf("heap->next: %p - ", heap->next ? heap->next : NULL);
    serial_printf("heap->prev: %p - ", heap->prev ? heap->prev : NULL);
    serial_printf("heap->size: %u\n", heap->size);

    heap = (heap_node_p)heap->next;
  }
}

void init_heap(void* addr, uint32_t size) {
  heap = (heap_node_p)addr;
  heap->magic = HEAP_MAGIC;
  heap->size = size - HEAP_HEADER_SIZE;
  heap->prev = NULL;
  heap->next = NULL;
  heap->status = HEAP_FREE;

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

int find_first_free_node(size_t size, heap_node_p* node) {
  *node = (heap_node_p)heap;

  serial_printf("find_first_free_node: node @ %p\n", *node);

  while(*node) {
    serial_printf("find_first_free_node: node->magic: %lx - node->status = %s(%d) - node->size: %d - requested: %d\n", (*node)->magic, (*node)->status == HEAP_FREE ? "FREE" : "USED", (*node)->status, (*node)->size, (size + HEAP_HEADER_SIZE));
    if((*node)->status == HEAP_FREE && (*node)->size >= (size + HEAP_HEADER_SIZE)) {
      return 0;
    }
    *node = (*node)->next;
  }

  return -1;
}

void* kmalloc(size_t size) {
  if(size == 0) {
    serial_printf("kmalloc: size is 0\n");
    return NULL;
  }

  heap_node_p node = NULL;
  heap_node_p next = NULL;

  if(find_first_free_node(size, &node) != 0) {
    serial_printf("kmalloc: no free node found\n");
    return NULL;
  }

  next = (heap_node_p)(((uintptr_t)node) + HEAP_HEADER_SIZE);
  serial_printf("kmalloc: next @ %p\n", next);

  next->magic = HEAP_MAGIC;
  next->size = node->size - (size + HEAP_HEADER_SIZE);
  next->prev = node;
  next->next = node->next;
  next->status = HEAP_FREE;

  node->status = HEAP_USED;
  node->size = size;
  node->next = next;

  debug_heap(heap);

  return (void*)node->data;
}

void kfree(void* ptr) {
  if (ptr == NULL) {
    return;
  }
}