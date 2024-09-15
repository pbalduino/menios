#ifndef MENIOS_INCLUDE_KERNEL_HEAP_H
#define MENIOS_INCLUDE_KERNEL_HEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#define HEAP_SIZE   16
#define HEAP_FREE    0
#define HEAP_USED    1
#define HEAP_MAGIC  "mOS\0"

typedef struct {
  uint8_t            magic[4]; // 4 bytes
  uint8_t            status;   // 1 byte
  uint32_t           size;     // 4 bytes
  struct heap_node*  next;     // 8 bytes
  struct heap_node*  prev;     // 8 bytes
  uint8_t*           data;             
} heap_node_t; //        header: 24 bytes

typedef heap_node_t* heap_node_p;

#define HEAP_HEADER offsetof(heap_node_t, data)

void* kmalloc(uint32_t size);
void* kcalloc(uint64_t nelem, uint64_t elsize);
void* krealloc(void* ptr, uint64_t size);
void kfree(void* ptr);
void* kmem_align(uint64_t size);

#ifdef __cplusplus
}
#endif

#endif
