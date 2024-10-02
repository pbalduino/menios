#ifndef MENIOS_INCLUDE_KERNEL_HEAP_H
#define MENIOS_INCLUDE_KERNEL_HEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#define HEAP_SIZE   16
#define HEAP_FREE    0
#define HEAP_USED    1
#define HEAP_MAGIC  0x534f6d00

typedef uint32_t HEAP_INSPECT_RESULT;

#define HEAP_INSPECT_OK            0
#define HEAP_INSPECT_CORRUPTED     1
#define HEAP_INSPECT_INVALID_INDEX 2

struct heap_node_t;

typedef struct heap_node_t {
  uint32_t             magic;  // 4 bytes
  uint8_t              status; // 1 byte
  uint32_t             size;   // 4 bytes
  struct heap_node_t*  next;   // 8 bytes
  struct heap_node_t*  prev;   // 8 bytes
  uint8_t              data[];
} /*__attribute__((packed))*/ heap_node_t; 

typedef struct heap_node_t* heap_node_p;

#define HEAP_HEADER_SIZE offsetof(heap_node_t, data) 

HEAP_INSPECT_RESULT inspect_heap(uint32_t node_index, heap_node_p* node);

void init_heap(void* addr, size_t size);

void* kmalloc(size_t size);
// void* kcalloc(uint64_t nelem, uint64_t elsize);
// void* krealloc(void* ptr, uint64_t size);
void kfree(void* ptr);
// void* kmem_align(uint64_t size);

#ifdef __cplusplus
}
#endif

#endif
