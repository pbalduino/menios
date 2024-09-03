#ifndef MENIOS_INCLUDE_KERNEL_KHEAP_H
#define MENIOS_INCLUDE_KERNEL_KHEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#define KHEAP_SIZE   16
#define KHEAP_HEADER 16
#define KHEAP_FREE    0
#define KHEAP_USED    1
#define KHEAP_MAGIC  "mOS"

struct kheap_node {
  uint8_t magic[3];        // 3 bytes
  uint8_t  status;         // 1 byte
  uint32_t size;           // 4 bytes
  struct kheap_node* next; // 8 bytes
  uint8_t* data;             
}; //                header: 16 bytes

void* kmalloc(uint32_t size);
void* kcalloc(uint64_t nelem, uint64_t elsize);
void* krealloc(void* ptr, uint64_t size);
void kfree(void* ptr);
void* kmem_align(uint64_t size);

#ifdef __cplusplus
}
#endif

#endif
