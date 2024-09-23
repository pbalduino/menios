#include <kernel/heap.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

#include <stddef.h>
#include <string.h>
#include <types.h>

#include <sys/mman.h>


void init_heap() {
  heap_node_p heap = (heap_node_p)current->heap;
  serial_printf("init_heap: heap @ %lx\n", heap);
  heap->magic = HEAP_MAGIC;
  heap->next = NULL;
  heap->prev = NULL;
  heap->size = PAGE_SIZE - HEAP_HEADER;
  heap->status = HEAP_FREE;
  // heap->data = (void*)(((uintptr_t)heap) + HEAP_HEADER);

  serial_printf("init_heap: OK\n");
}

void check_heap() {
  if(current->heap == current->brk) {
    serial_printf("check_heap: initializing heap\n");
    current->heap = mmap(NULL, 0, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }

  serial_printf("check_heap: current->heap @ %lx\n", current->heap);

  heap_node_p heap = (heap_node_p)current->heap;

  serial_printf("check_heap: checking heap magic\n");
  if(heap->size == 0 && heap->magic == HEAP_MAGIC) {
    init_heap();
  }
  serial_printf("check_heap: OK\n");
}

void debug_heap() {
  heap_node_p heap = (heap_node_p)current->heap;

  serial_printf("heap debug:\n");
  while(heap) {
    serial_printf("heap @ %p - ", heap);
    serial_printf("heap->magic: %s(%s) - %lx -", heap->magic == HEAP_MAGIC ? "OK " : "BAD", heap->magic, heap->magic);
    serial_printf("heap->status: %s - ", heap->status == HEAP_FREE ? "FREE" : "USED");
    serial_printf("heap->data: %p - ", heap->data);
    serial_printf("heap->next: %p - ", heap->next ? heap->next : "NULL");
    serial_printf("heap->prev: %p - ", heap->prev ? heap->prev : "NULL");
    serial_printf("heap->size: %u\n", heap->size);

    heap = (heap_node_p)heap->next;
  }
}

void init_next_node(heap_node_p heap, heap_node_p next, size_t size, size_t total_size, int mark_as_used) {
  if(next == NULL) {
    serial_printf("init_next_node: next is NULL\n");
  }
  next->size = total_size - (size + HEAP_HEADER);
  next->status = HEAP_FREE;
  next->prev = heap;
  next->next = NULL;
  // next->data = (void*)(((uintptr_t)next) + HEAP_HEADER);
  next->magic = HEAP_MAGIC;
  
  heap->next = next;
  heap->size = size;
  if (mark_as_used) {
    heap->status = HEAP_USED;
  }
}

void* malloc(size_t size) {
  if(size == 0) {
    return NULL;
  }

  check_heap();
  serial_printf("malloc: requested size: %d\n", size);

  heap_node_p heap = (heap_node_p)current->heap;

  serial_printf("malloc: heap @ %lx\n", heap);

  while (heap) {
    if (heap->size >= size && heap->status == HEAP_FREE) {
      serial_printf("malloc: found heap node @ %lx with size %d\n", heap, size);
      serial_printf("malloc: node data @ %lx\n", heap->data);

      heap_node_p next = (heap_node_p)(((uintptr_t)heap->data) + size);

      init_next_node(heap, next, size, heap->size, 1); // Mark as used

      serial_printf("malloc: returning block starting @ %lx with size: %d\n", heap->data, heap->size);
      debug_heap();
      return (void*)heap->data;
    }

    if (heap->size < size && heap->status == HEAP_FREE) {
      serial_printf("malloc: found a small node. skipping.\n");
    } else if (heap->status == HEAP_USED) {
      serial_printf("malloc: found used node. skipping.\n");
    }

    if (!heap->next) {
      serial_printf("malloc: no free heap node found, allocating new page\n");

      void* extra_mem = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

      if (extra_mem == MAP_FAILED) {
        serial_printf("malloc: mmap failed\n");
        return NULL;
      }

      if (heap->status == PAGE_FREE) {
        serial_printf("malloc: extending heap node\n");
        uintptr_t heap_end = (uintptr_t)heap->data + heap->size;
        uintptr_t new_mem = (uintptr_t)extra_mem;

        if (heap_end == new_mem) {
          serial_printf("malloc: heap node is adjacent to new page\n");
          heap->size += PAGE_SIZE;
          debug_heap();

          heap_node_p next = (heap_node_p)(((uintptr_t)heap->data) + heap->size);
          init_next_node(heap, next, size, heap->size, 1); // Adjacent, mark as used
          return (void*)next->data;
        }

        serial_printf("malloc: heap node is not adjacent to new page\n");
        heap_node_p next = (heap_node_p)extra_mem;

        init_next_node(heap, next, size, PAGE_SIZE, 1); // Non-adjacent, mark next as used, but not current heap

        heap->status = PAGE_FREE; // Keep the current heap node as free
        debug_heap();
        serial_printf("malloc: allocating new heap node on another page\n");
        return (void*)next->data;
      }
    }

    heap = heap->next;
  }

  serial_printf("malloc: no free heap node found\n");
  debug_heap();
  return NULL;
}

void free(void* ptr) {
  if (ptr == NULL) {
    return; // Do nothing if pointer is NULL
  }

  // Get the heap node from the data pointer by subtracting the HEAP_HEADER size
  heap_node_p node = (heap_node_p)(((uintptr_t)ptr) - HEAP_HEADER);

  serial_printf("free: freeing memory block @ %lx\n", ptr);

  // Check if the node is valid and marked as used
  if (node->magic == HEAP_MAGIC) {
    serial_printf("free: invalid heap node magic, memory corruption detected: %lx\n", node->magic);
    return; // Invalid memory block
  } 

  if (node->status != HEAP_USED) {
    serial_printf("free: attempting to free an already free block\n");
    return; // Block is already free
  }

  heap_node_p next = node->next;
  heap_node_p prev = node->prev;

  // Mark the current node as free
  node->status = HEAP_FREE;
  serial_printf("free: node @ %lx marked as free\n", node);

  // Try to coalesce with the next node if it's free
  if (next && next->status == HEAP_FREE) {
    serial_printf("free: merging with next free node @ %lx\n", next);
    node->size += sizeof(heap_node_t) + next->size;
    node->next = next->next;
    if (node->next) {
      ((heap_node_p)node->next)->prev = node;
    }
  }

  // Try to coalesce with the previous node if it's free
  if (prev && prev->status == HEAP_FREE) {
    serial_printf("free: merging with previous free node @ %lx\n", prev);
    prev->size += sizeof(heap_node_t) + node->size;
    prev->next = node->next;
    if (node->next) {
      ((heap_node_p)node->next)->prev = prev;
    }
  }

  debug_heap(); // Optionally debug the heap state after freeing
}