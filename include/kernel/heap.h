#ifndef MENIOS_INCLUDE_KERNEL_HEAP_H
#define MENIOS_INCLUDE_KERNEL_HEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#ifdef MENIOS_HOST_TEST
#include <stdbool.h>
#endif

#define HEAP_SIZE   0x400
#define HEAP_FREE    0
#define HEAP_USED    1
#define HEAP_MAGIC  0x534f6d00 // mOS

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
} heap_node_t; 

typedef struct heap_node_t* heap_node_p;

#define HEAP_HEADER_SIZE offsetof(heap_node_t, data) 

typedef struct heap_stats_t {
  size_t total_bytes;
  size_t free_bytes;
  size_t used_bytes;
  size_t region_count;
} heap_stats_t;

HEAP_INSPECT_RESULT inspect_heap(uint32_t node_index, heap_node_p* node);

void heap_init(void* addr, size_t size);

void* kmalloc(size_t size);
void* kcalloc(size_t nelem, size_t elsize);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);
// void* kmem_align(uint64_t size);

heap_stats_t heap_get_stats(void);

void dump_heap(heap_node_p heap, size_t size);

void heap_compactor();

#ifdef MENIOS_HOST_TEST
void __kmalloc_debug_reset_virtual(void);
bool __kmalloc_debug_reserve_range(size_t bytes, virt_addr_t* out_vaddr);
void __kmalloc_debug_release_range(virt_addr_t base, size_t size);
size_t __kmalloc_debug_free_range_count(void);
virt_addr_t __kmalloc_debug_next_vaddr(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
