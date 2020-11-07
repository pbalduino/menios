#ifndef MENIOS_INCLUDE_KERNEL_KHEAP_H
#define MENIOS_INCLUDE_KERNEL_KHEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

typedef struct {
  bool free;
  uint32_t start;
  uint32_t size;
} heap_node_t;

typedef struct {
  uint32_t start;
  uint32_t end;
  heap_node_t nodes[64];
} heap_t;

void* kmalloc(size_t size);
void* kcalloc(size_t nelem, size_t elsize);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);
void* kmem_align(size_t size);

#define NEWA(t, s) kmalloc(sizeof(t) * s)
#define NEW(t) (NEWA(t, 1))

#ifdef __cplusplus
}
#endif

#endif
