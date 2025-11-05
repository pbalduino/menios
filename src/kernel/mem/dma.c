#include <kernel/dma.h>
#include <kernel/heap.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <string.h>

static size_t round_up_to_page_count(size_t size) {
  return (size + (PAGE_SIZE - 1)) / PAGE_SIZE;
}

bool dma_buffer_alloc(dma_buffer_t* buffer,
                      size_t size,
                      size_t alignment,
                      phys_addr_t max_phys_addr,
                      bool zero) {
  if(buffer == NULL || size == 0) {
    return false;
  }

  size_t page_count = round_up_to_page_count(size);
  size_t align_bytes = alignment == 0 ? DMA_DEFAULT_ALIGNMENT : alignment;

  if(align_bytes % PAGE_SIZE != 0) {
    align_bytes = ((align_bytes + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
  }

  phys_addr_t max_phys = max_phys_addr == 0 ? DMA_DEFAULT_MAX_PHYS : max_phys_addr;
  phys_frame_t base_frame = pmm_alloc_aligned_pages(page_count, align_bytes, max_phys);
  if(!phys_frame_is_valid(base_frame)) {
    serial_printf("dma_buffer_alloc: failed (size=%zu alignment=%zu max=%llx)\n",
                  size,
                  align_bytes,
                  (unsigned long long)max_phys);
    return false;
  }

  phys_addr_t base_phys = phys_frame_to_addr(base_frame);
  void* virt = (void*)physical_to_virtual(base_phys);
  size_t total_size = page_count * PAGE_SIZE;

  if(zero) {
    memset(virt, 0, total_size);
  }

  buffer->virt = virt;
  buffer->phys = base_phys;
  buffer->size = total_size;
  buffer->page_count = page_count;
  return true;
}

void dma_buffer_free(dma_buffer_t* buffer) {
  if(buffer == NULL || buffer->page_count == 0) {
    return;
  }

  pmm_free_pages(phys_frame_from_addr(buffer->phys), buffer->page_count);
  buffer->virt = NULL;
  buffer->phys = 0;
  buffer->size = 0;
  buffer->page_count = 0;
}
