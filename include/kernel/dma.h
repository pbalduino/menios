#ifndef MENIOS_INCLUDE_KERNEL_DMA_H
#define MENIOS_INCLUDE_KERNEL_DMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/pmm.h>

#define DMA_DEFAULT_ALIGNMENT 4096
#define DMA_DEFAULT_MAX_PHYS 0xFFFFFFFFull

typedef struct dma_buffer_t {
  void*       virt;
  phys_addr_t phys;
  size_t      size;
  size_t      page_count;
} dma_buffer_t;

bool dma_buffer_alloc(dma_buffer_t* buffer,
                      size_t size,
                      size_t alignment,
                      phys_addr_t max_phys_addr,
                      bool zero);

void dma_buffer_free(dma_buffer_t* buffer);

static inline bool dma_alloc_default(dma_buffer_t* buffer, size_t size) {
  return dma_buffer_alloc(buffer, size, DMA_DEFAULT_ALIGNMENT, DMA_DEFAULT_MAX_PHYS, true);
}

#ifdef __cplusplus
}
#endif

#endif
