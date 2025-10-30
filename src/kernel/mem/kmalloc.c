#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <types.h>

#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

#define HEAP_ALIGNMENT      16UL
#define HEAP_MINIMUM_PAGES   1UL
#define HEAP_REGION_CAP     8192UL

#define KHEAP_BASE  0xffffc00000000000ull
#define KHEAP_SIZE  (256ull * 1024 * 1024ull)
#define KHEAP_LIMIT (KHEAP_BASE + KHEAP_SIZE)

typedef struct heap_region_t heap_region_t;

struct heap_region_t {
  heap_node_p      base;
  size_t           size_bytes;
  phys_addr_t      phys_base;
  size_t           page_count;
  bool             managed;
  heap_region_t*   next;
};

static bool            heap_freed = false;
static heap_node_p     heap;
static heap_node_p     heap_tail;
static spinlock_t      heap_lock;
static heap_region_t*  heap_regions_head;
static heap_region_t*  heap_regions_tail;
static heap_region_t   heap_region_entries[HEAP_REGION_CAP];
static bool            heap_region_used[HEAP_REGION_CAP];
static virt_addr_t     heap_next_vaddr = KHEAP_BASE;

typedef struct heap_vrange_t {
  virt_addr_t base;
  size_t      size;
  struct heap_vrange_t* next;
} heap_vrange_t;

static heap_vrange_t   heap_vrange_entries[HEAP_REGION_CAP];
static bool            heap_vrange_used[HEAP_REGION_CAP];
static heap_vrange_t*  heap_vrange_head;

static void heap_virtual_reset(void);
static heap_vrange_t* heap_vrange_alloc(void);
static void heap_vrange_free(heap_vrange_t* entry);
static bool heap_virtual_acquire(size_t bytes, virt_addr_t* out, bool* used_free);
static void heap_virtual_release(virt_addr_t base, size_t bytes);

static inline void heap_reset_lock(void) {
  spinlock_init(&heap_lock);
  heap_next_vaddr = KHEAP_BASE;
  heap_virtual_reset();
}

static void* heap_map_region(phys_addr_t phys_base, size_t page_count) {
  size_t bytes = page_count * PAGE_SIZE;
  bool used_free_range = false;
  virt_addr_t virt;

  if(!heap_virtual_acquire(bytes, &virt, &used_free_range)) {
    serial_printf("heap_map_region: virtual arena exhausted (requested %zu bytes)\n", bytes);
    return NULL;
  }

  phys_addr_t root = read_cr3();

  for(size_t page = 0; page < page_count; page++) {
    phys_addr_t phys = phys_base + (page * PAGE_SIZE);
    virt_addr_t vaddr = virt + (page * PAGE_SIZE);
    if(!pmm_map_page(vaddr, phys, true, false)) {
      serial_printf("heap_map_region: map failed at %lx\n", (unsigned long)vaddr);
      for(size_t rollback = 0; rollback < page; ++rollback) {
        virt_addr_t rollback_vaddr = virt + (rollback * PAGE_SIZE);
        if(!pmm_remove_mapping_in_root(root, rollback_vaddr)) {
          serial_printf("heap_map_region: rollback failed at %lx\n", (unsigned long)rollback_vaddr);
        }
      }
      if(used_free_range) {
        heap_virtual_release(virt, bytes);
      }
      return NULL;
    }
  }

  if(!used_free_range) {
    heap_next_vaddr += bytes;
  }

  return (void*)virt;
}

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
  disable_interrupts();
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
  enable_interrupts();
#endif
}

static inline size_t align_down(size_t value, size_t alignment) {
  return value & ~(alignment - 1);
}

static inline size_t align_up(size_t value, size_t alignment) {
  return (value + (alignment - 1)) & ~(alignment - 1);
}

static heap_region_t* heap_allocate_region_entry(void) {
  for(size_t idx = 0; idx < HEAP_REGION_CAP; idx++) {
    if(!heap_region_used[idx]) {
      heap_region_used[idx] = true;
      heap_region_entries[idx].next = NULL;
      return &heap_region_entries[idx];
    }
  }

  return NULL;
}

static void heap_free_region_entry(heap_region_t* region) {
  for(size_t idx = 0; idx < HEAP_REGION_CAP; idx++) {
    if(&heap_region_entries[idx] == region) {
      heap_region_used[idx] = false;
      heap_region_entries[idx].next = NULL;
      return;
    }
  }
}

static void heap_unmap_pages(virt_addr_t base, size_t page_count) {
  phys_addr_t root = read_cr3();
  for(size_t page = 0; page < page_count; ++page) {
    virt_addr_t vaddr = base + (page * PAGE_SIZE);
    if(!pmm_unmap_page_in_root(root, vaddr)) {
      serial_printf("heap_unmap_pages: failed to unmap %lx\n", (unsigned long)vaddr);
    }
  }
}

static void heap_reset_regions(void) {
  memset(heap_region_used, 0, sizeof(heap_region_used));
  heap_regions_head = NULL;
  heap_regions_tail = NULL;
}

static void heap_virtual_reset(void) {
  memset(heap_vrange_used, 0, sizeof(heap_vrange_used));
  heap_vrange_head = NULL;
}

static heap_vrange_t* heap_vrange_alloc(void) {
  for(size_t idx = 0; idx < HEAP_REGION_CAP; ++idx) {
    if(!heap_vrange_used[idx]) {
      heap_vrange_used[idx] = true;
      heap_vrange_entries[idx].next = NULL;
      return &heap_vrange_entries[idx];
    }
  }
  serial_printf("heap_virtual: descriptor pool exhausted\n");
  return NULL;
}

static void heap_vrange_free(heap_vrange_t* entry) {
  if(entry == NULL) {
    return;
  }
  size_t idx = (size_t)(entry - heap_vrange_entries);
  if(idx < HEAP_REGION_CAP) {
    heap_vrange_used[idx] = false;
    heap_vrange_entries[idx].next = NULL;
  }
}

static void heap_virtual_release(virt_addr_t base, size_t bytes) {
  if(bytes == 0) {
    return;
  }

  virt_addr_t start = base;
  (void)start;

  heap_vrange_t* prev = NULL;
  heap_vrange_t* curr = heap_vrange_head;

  while(curr && curr->base < start) {
    prev = curr;
    curr = curr->next;
  }

  heap_vrange_t* target = NULL;

  if(prev && prev->base + prev->size == start) {
    prev->size += bytes;
    target = prev;
  } else {
    heap_vrange_t* entry = heap_vrange_alloc();
    if(entry == NULL) {
      return;
    }
    entry->base = start;
    entry->size = bytes;
    entry->next = curr;
    if(prev) {
      prev->next = entry;
    } else {
      heap_vrange_head = entry;
    }
    target = entry;
  }

  while(target->next && (target->base + target->size) == target->next->base) {
    heap_vrange_t* next = target->next;
    target->size += next->size;
    target->next = next->next;
    heap_vrange_free(next);
  }
}

static bool heap_virtual_acquire(size_t bytes, virt_addr_t* out, bool* used_free) {
  if(bytes == 0 || out == NULL || used_free == NULL) {
    return false;
  }

  heap_vrange_t* prev = NULL;
  heap_vrange_t* curr = heap_vrange_head;

  while(curr) {
    if(curr->size >= bytes) {
      virt_addr_t base = curr->base;
      if(curr->size == bytes) {
        if(prev) {
          prev->next = curr->next;
        } else {
          heap_vrange_head = curr->next;
        }
        heap_vrange_free(curr);
      } else {
        curr->base += bytes;
        curr->size -= bytes;
      }
      *used_free = true;
      *out = base;
      return true;
    }
    prev = curr;
    curr = curr->next;
  }

  if(heap_next_vaddr + bytes > KHEAP_LIMIT) {
    return false;
  }

  *used_free = false;
  *out = heap_next_vaddr;
  return true;
}

static heap_region_t* heap_register_region(heap_node_p base,
                                           size_t size_bytes,
                                           phys_addr_t phys_base,
                                           size_t page_count,
                                           bool managed) {
  heap_region_t* region = heap_allocate_region_entry();

  if(region == NULL) {
    serial_printf("heap_register_region: descriptor pool exhausted\n");
    return NULL;
  }

  region->base = base;
  region->size_bytes = size_bytes;
  region->phys_base = phys_base;
  region->page_count = page_count;
  region->managed = managed;
  region->next = NULL;

  if(heap_regions_tail) {
    heap_regions_tail->next = region;
  } else {
    heap_regions_head = region;
  }

  heap_regions_tail = region;

  return region;
}

static void heap_unregister_region(heap_region_t* region) {
  if(region == NULL) {
    return;
  }

  heap_region_t* prev = NULL;
  heap_region_t* cursor = heap_regions_head;

  while(cursor) {
    if(cursor == region) {
      if(prev) {
        prev->next = cursor->next;
      } else {
        heap_regions_head = cursor->next;
      }

      if(heap_regions_tail == cursor) {
        heap_regions_tail = prev;
      }

      heap_free_region_entry(cursor);
      return;
    }

    prev = cursor;
    cursor = cursor->next;
  }
}

static heap_region_t* heap_region_from_node(heap_node_p node) {
  uintptr_t address = (uintptr_t)node;
  heap_region_t* region = heap_regions_head;

  while(region) {
    uintptr_t base = (uintptr_t)region->base;
    uintptr_t limit = base + region->size_bytes;

    if(address >= base && address < limit) {
      return region;
    }

    region = region->next;
  }

  return NULL;
}

static bool nodes_are_contiguous(heap_node_p left, heap_node_p right) {
  if(left == NULL || right == NULL) {
    return false;
  }

  uintptr_t expected = (uintptr_t)left + HEAP_HEADER_SIZE + left->size;
  return expected == (uintptr_t)right;
}

static size_t heap_calculate_free_bytes(void) {
  size_t free_bytes = 0;
  heap_node_p cursor = heap;

  while(cursor) {
    if(cursor->magic == HEAP_MAGIC && cursor->status == HEAP_FREE) {
      free_bytes += cursor->size;
    }
    cursor = cursor->next;
  }

  return free_bytes;
}

static bool heap_region_contains_other_nodes(heap_region_t* region,
                                             heap_node_p excluded) {
  if(region == NULL) {
    return false;
  }

  uintptr_t base = (uintptr_t)region->base;
  uintptr_t limit = base + region->size_bytes;

  heap_node_p cursor = heap;

  while(cursor) {
    uintptr_t address = (uintptr_t)cursor;
    if(cursor != excluded && address >= base && address < limit) {
      return true;
    }
    cursor = cursor->next;
  }

  return false;
}

static void heap_release_region_if_unused(heap_node_p node) {
  heap_region_t* region = heap_region_from_node(node);

  if(region == NULL || !region->managed) {
    return;
  }

  if(node->status != HEAP_FREE) {
    return;
  }

  if((uintptr_t)node != (uintptr_t)region->base) {
    return;
  }

  if(node->size + HEAP_HEADER_SIZE != region->size_bytes) {
    return;
  }

  if(heap_region_contains_other_nodes(region, node)) {
    return;
  }

  heap_node_p prev = node->prev;
  heap_node_p next = node->next;

  if(prev) {
    prev->next = next;
  } else {
    heap = next;
    if(heap != NULL) {
      heap->prev = NULL;
    }
  }

  if(next != NULL) {
    next->prev = prev;
  }

  if(heap_tail == node) {
    heap_tail = prev;
  }

  node->prev = NULL;
  node->next = NULL;

  virt_addr_t base = (virt_addr_t)region->base;
  heap_unmap_pages(base, region->page_count);

  heap_virtual_release(base, region->size_bytes);
  heap_unregister_region(region);
}

static bool heap_grow(size_t minimum_size) {
  size_t requested = minimum_size ? minimum_size : PAGE_SIZE;
  requested = align_up(requested, PAGE_SIZE);

  size_t page_count = requested / PAGE_SIZE;
  if(page_count < HEAP_MINIMUM_PAGES) {
    page_count = HEAP_MINIMUM_PAGES;
    requested = page_count * PAGE_SIZE;
  }

  phys_addr_t phys_base = pmm_alloc_pages(page_count);
  if(phys_base == 0) {
    serial_printf("heap_grow: unable to allocate %zu pages\n", page_count);
    return false;
  }

  void* base_address = heap_map_region(phys_base, page_count);
  if(base_address == NULL) {
    pmm_free_pages(phys_base, page_count);
    return false;
  }

  memset(base_address, 0, requested);

  heap_node_p node = (heap_node_p)base_address;
  node->magic = HEAP_MAGIC;
  node->size = requested - HEAP_HEADER_SIZE;
  node->next = NULL;
  node->prev = NULL;
  node->status = HEAP_FREE;

  heap_node_p previous_tail = heap_tail;

  if(heap == NULL) {
    heap = node;
  }

  if(previous_tail) {
    previous_tail->next = node;
    node->prev = previous_tail;
  } else {
    node->prev = NULL;
  }

  heap_tail = node;

  if(heap_register_region(node, requested, phys_base, page_count, true) == NULL) {
    serial_printf("heap_grow: failed to register region\n");

    if(previous_tail) {
      previous_tail->next = NULL;
      heap_tail = previous_tail;
    } else {
      heap = NULL;
      heap_tail = NULL;
    }

    node->prev = NULL;
    node->next = NULL;

    heap_unmap_pages((virt_addr_t)node, page_count);
    heap_virtual_release((virt_addr_t)node, requested);

    pmm_free_pages(phys_base, page_count);
    return false;
  }

  return true;
}

static void heap_split_node(heap_node_p node, size_t requested_size) {
  size_t available = node->size;

  if(available >= requested_size + HEAP_HEADER_SIZE + HEAP_ALIGNMENT) {
    heap_node_p old_next = node->next;
    heap_node_p next = (heap_node_p)(((uintptr_t)node) + HEAP_HEADER_SIZE + requested_size);
    next->magic = HEAP_MAGIC;
    next->size = available - requested_size - HEAP_HEADER_SIZE;
    next->status = HEAP_FREE;
    next->next = old_next;
    next->prev = node;
    if(old_next != NULL) {
      old_next->prev = next;
    }

    node->size = requested_size;
    node->next = next;

    if(heap_tail == node) {
      heap_tail = next;
    }
  } else {
    node->size = available;
  }

  node->status = HEAP_USED;
}

static heap_node_p heap_find_suitable_node(size_t size) {
  heap_node_p candidate = heap;

  while(candidate) {
    if(candidate->magic != HEAP_MAGIC) {
      serial_printf("heap_find_suitable_node: corrupted node at %p\n", candidate);
      return NULL;
    }

    if(candidate->status == HEAP_FREE && candidate->size >= size) {
      return candidate;
    }

    candidate = candidate->next;
  }

  return NULL;
}

static void heap_set_errno(int err) {
  if(current) {
    current->err_no = err;
  }
}

void heap_init(void* addr, size_t size) {
  heap_reset_lock();
  heap_freed = false;
  heap = NULL;
  heap_tail = NULL;
  heap_reset_regions();

  if(addr != NULL) {
    uintptr_t base = (uintptr_t)addr;
    size_t aligned_size = align_down(size, PAGE_SIZE);

    if(aligned_size < PAGE_SIZE) {
      serial_printf("heap_init: region too small (%lu)\n", size);
      return;
    }

    heap_node_p node = (heap_node_p)base;
    memset(node, 0, aligned_size);
    node->magic = HEAP_MAGIC;
    node->size = aligned_size - HEAP_HEADER_SIZE;
    node->status = HEAP_FREE;
    node->next = NULL;
    node->prev = NULL;

    heap = node;
    heap_tail = node;

    if(heap_register_region(node, aligned_size, 0, aligned_size / PAGE_SIZE, false) == NULL) {
      serial_printf("heap_init: failed to register static region\n");
    }

    serial_printf("Heap initialized at %p with size %ld\n", addr, (long)aligned_size);
    return;
  }

  if(size == 0) {
    size = PAGE_SIZE * HEAP_MINIMUM_PAGES;
  }

  if(!heap_grow(size)) {
    serial_printf("heap_init: unable to reserve %lu bytes\n", size);
  } else {
    serial_printf("Heap initialized dynamically (%lu bytes)\n", align_up(size, PAGE_SIZE));
  }
}

HEAP_INSPECT_RESULT inspect_heap(uint32_t node_index, heap_node_p* node) {
  if(heap == NULL) {
    return HEAP_INSPECT_INVALID_INDEX;
  }

  *node = heap;
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

/**
 * Returns null if size == 0 or if there's no memory
 */
void* kmalloc(size_t size) {
  if(size == 0) {
    serial_printf("kmalloc: size is 0\n");
    return NULL;
  }

  size = align_up(size, HEAP_ALIGNMENT);

  spinlock_lock(&heap_lock);

  heap_node_p node = heap_find_suitable_node(size);

  if(node == NULL) {
    if(!heap_grow(size + HEAP_HEADER_SIZE)) {
      serial_printf("kmalloc: failed to grow heap (need %lu bytes, have %lu)\n",
        size,
        heap_calculate_free_bytes());
      heap_set_errno(ENOMEM);
      spinlock_unlock(&heap_lock);
      return NULL;
    }

    node = heap_find_suitable_node(size);

    if(node == NULL) {
      serial_printf("kmalloc: no block even after grow\n");
      heap_set_errno(ENOMEM);
      spinlock_unlock(&heap_lock);
      return NULL;
    }
  }

  heap_split_node(node, size);

  spinlock_unlock(&heap_lock);

  return (void*)node->data;
}

void* kcalloc(size_t nelem, size_t elsize) {
  if(nelem == 0 || elsize == 0) {
    return NULL;
  }

  if(elsize != 0 && nelem > ((size_t)-1) / elsize) {
    heap_set_errno(ENOMEM);
    return NULL;
  }

  size_t total = nelem * elsize;
  void* ptr = kmalloc(total);

  if(ptr) {
    memzero(ptr, total);
  }

  return ptr;
}

void* krealloc(void* ptr, size_t size) {
  if(ptr == NULL) {
    return kmalloc(size);
  }

  if(size == 0) {
    kfree(ptr);
    return NULL;
  }

  heap_node_p node = (heap_node_p)((uintptr_t)ptr - HEAP_HEADER_SIZE);
  size_t old_size = 0;

  spinlock_lock(&heap_lock);

  if(node->magic != HEAP_MAGIC) {
    serial_printf("krealloc: invalid pointer %p\n", ptr);
    spinlock_unlock(&heap_lock);
    heap_set_errno(EINVAL);
    return NULL;
  }

  old_size = node->size;
  spinlock_unlock(&heap_lock);

  if(size <= old_size) {
    return ptr;
  }

  void* new_ptr = kmalloc(size);

  if(new_ptr == NULL) {
    return NULL;
  }

  memcpy(new_ptr, ptr, old_size);
  kfree(ptr);

  return new_ptr;
}

static void heap_merge_forward(heap_node_p node) {
  while(node->next && node->next->status == HEAP_FREE && nodes_are_contiguous(node, node->next)) {
    heap_node_p next = node->next;
    node->size += next->size + HEAP_HEADER_SIZE;
    node->next = next->next;
    if(node->next != NULL) {
      node->next->prev = node;
    }

    if(heap_tail == next) {
      heap_tail = node;
    }
  }
}

void kfree(void* ptr) {
  if(ptr == NULL) {
    return;
  }

  heap_node_p node = (heap_node_p)((uintptr_t)ptr - HEAP_HEADER_SIZE);

  spinlock_lock(&heap_lock);

  if(node->magic != HEAP_MAGIC) {
    serial_printf("kfree: invalid pointer %p\n", ptr);
    spinlock_unlock(&heap_lock);
    return;
  }

  if(node->status == HEAP_FREE) {
    serial_printf("kfree: double free detected at %p\n", ptr);
    spinlock_unlock(&heap_lock);
    return;
  }

  node->status = HEAP_FREE;
  heap_freed = true;

  heap_node_p prev = node->prev;

  if(prev && prev->status == HEAP_FREE && nodes_are_contiguous(prev, node)) {
    prev->size += node->size + HEAP_HEADER_SIZE;
    prev->next = node->next;
    if(node->next != NULL) {
      node->next->prev = prev;
    }
    if(heap_tail == node) {
      heap_tail = prev;
    }
    node = prev;
  }

  heap_merge_forward(node);
  if(node->next != NULL) {
    node->next->prev = node;
  }
  heap_release_region_if_unused(node);

  spinlock_unlock(&heap_lock);
}

void heap_compactor() {
  serial_printf("heap_compactor: initing\n");
  spinlock_lock(&heap_lock);

  if(!heap_freed) {
    spinlock_unlock(&heap_lock);
    serial_printf("heap_compactor: nothing to do here.\n");
    return;
  }

  heap_node_p node = heap;

  while(node) {
    if(node->status == HEAP_FREE) {
      heap_merge_forward(node);
      memzero(node->data, node->size);
      serial_printf("heap_compactor: Compacted @ %p.\n", node);
    }

    node = node->next;
  }

  heap_freed = false;
  spinlock_unlock(&heap_lock);
  serial_printf("heap_compactor: leaving.\n");
}

heap_stats_t heap_get_stats(void) {
  heap_stats_t stats = {0};

  spinlock_lock(&heap_lock);

  heap_region_t* region = heap_regions_head;

  while(region) {
    if(region->size_bytes > HEAP_HEADER_SIZE) {
      stats.total_bytes += region->size_bytes - HEAP_HEADER_SIZE;
    }
    stats.region_count++;
    region = region->next;
  }

  heap_node_p cursor = heap;

  while(cursor) {
    if(cursor->magic == HEAP_MAGIC && cursor->status == HEAP_FREE) {
      stats.free_bytes += cursor->size;
    }

    cursor = cursor->next;
  }

  if(stats.total_bytes >= stats.free_bytes) {
    stats.used_bytes = stats.total_bytes - stats.free_bytes;
  } else {
    stats.used_bytes = 0;
  }

  spinlock_unlock(&heap_lock);

  return stats;
}

#ifdef MENIOS_HOST_TEST
static size_t heap_virtual_free_range_count(void) {
  size_t count = 0;
  heap_vrange_t* cursor = heap_vrange_head;
  while(cursor) {
    ++count;
    cursor = cursor->next;
  }
  return count;
}

void __kmalloc_debug_reset_virtual(void) {
  heap_virtual_reset();
  heap_next_vaddr = KHEAP_BASE;
}

bool __kmalloc_debug_reserve_range(size_t bytes, virt_addr_t* out_vaddr) {
  bool used_free = false;
  virt_addr_t base;
  if(!heap_virtual_acquire(bytes, &base, &used_free)) {
    return false;
  }
  if(!used_free) {
    heap_next_vaddr += bytes;
  }
  if(out_vaddr != NULL) {
    *out_vaddr = base;
  }
  return true;
}

void __kmalloc_debug_release_range(virt_addr_t base, size_t bytes) {
  heap_virtual_release(base, bytes);
}

size_t __kmalloc_debug_free_range_count(void) {
  return heap_virtual_free_range_count();
}

virt_addr_t __kmalloc_debug_next_vaddr(void) {
  return heap_next_vaddr;
}
#endif
