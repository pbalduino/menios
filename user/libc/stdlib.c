#include <ctype.h>
#include <limits.h>
#include <menios/syscall.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MENIOS_HOST_TEST
#include <menios/syscall_user.h>
#else
#include "allocator_debug.h"
#endif

void __menios_fini_libc(int status);

#ifdef MENIOS_HOST_TEST
void __menios_fini_libc(int status) {
  (void)status;
}

static inline long __menios_syscall0(long number) {
  (void)number;
  errno = ENOSYS;
  return -1;
}

static inline long __menios_syscall1(long number, long arg1) {
  (void)number;
  (void)arg1;
  errno = ENOSYS;
  return -1;
}

static inline long __menios_syscall2(long number, long arg1, long arg2) {
  (void)number;
  (void)arg1;
  (void)arg2;
  errno = ENOSYS;
  return -1;
}

static inline long __menios_syscall3(long number, long arg1, long arg2, long arg3) {
  (void)number;
  (void)arg1;
  (void)arg2;
  (void)arg3;
  errno = ENOSYS;
  return -1;
}
#endif

#define DEFAULT_ALIGNMENT   16u
#define ARENA_INITIAL_SIZE  (1u << 27)   /* 128 MiB payload */
#define ARENA_MAX_SIZE      (1u << 27)   /* 128 MiB payload */
#define BLOCK_FLAG_FREE     (1u << 0)
#define BLOCK_FLAG_DIRECT   (1u << 1)
#define BLOCK_FLAG_BUDDY    (1u << 2)

#define BUDDY_MIN_ORDER     7u           /* 128 bytes */
#define BUDDY_MAX_ORDER     27u          /* 128 MiB */
#define BUDDY_ORDER_COUNT   (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1u)

#define BUDDY_FLAG_FREE     (1u << 0)
#define BUDDY_FLAG_USED     (1u << 1)

struct arena_header;

typedef struct block_header {
  struct block_header* buddy_next;
  struct block_header* buddy_prev;
  struct arena_header* arena;     /* NULL for directly mapped blocks */
  uint32_t buddy_order;           /* valid for buddy managed blocks */
  uint32_t buddy_flags;
  uintptr_t buddy_offset;         /* byte offset from arena->buddy_base */
  void* mapping_base;             /* only used for direct mappings */
  size_t mapping_size;            /* only used for direct mappings */
  size_t size;                    /* payload size for this block */
  uint32_t flags;
  uint32_t padding_reserved;      /* reserved */
  uint64_t padding_align;         /* keep header aligned to 16 bytes */
} block_header_t;

typedef struct arena_header {
  struct arena_header* next;
  struct arena_header* prev;
  void* mapping_base;             /* raw mapping returned by mmap */
  size_t size;                    /* total bytes mapped (header + payload) */
  uint8_t* buddy_base;            /* start of buddy-managed payload */
  size_t buddy_size;              /* size of buddy-managed payload */
  block_header_t* buddy_freelists[BUDDY_ORDER_COUNT];
} arena_header_t;

_Static_assert((sizeof(block_header_t) % DEFAULT_ALIGNMENT) == 0,
               "block header must stay aligned");

static arena_header_t* arena_list_head = NULL;
static size_t direct_allocation_count = 0;
static size_t direct_total_bytes = 0;

static int grow_heap(size_t size);

static inline bool buddy_order_valid(uint32_t order) {
  return order >= BUDDY_MIN_ORDER && order <= BUDDY_MAX_ORDER;
}

static inline size_t buddy_order_index(uint32_t order) {
  return (size_t)(order - BUDDY_MIN_ORDER);
}

static inline size_t buddy_order_size(uint32_t order) {
  return (size_t)1u << order;
}

static inline block_header_t* buddy_block_from_offset(arena_header_t* arena, uintptr_t offset) {
  if(arena == NULL || offset >= arena->buddy_size) {
    return NULL;
  }
  return (block_header_t*)(arena->buddy_base + offset);
}

static block_header_t* buddy_materialize_block(arena_header_t* arena,
                                               uintptr_t offset,
                                               uint32_t order) {
  block_header_t* block = buddy_block_from_offset(arena, offset);
  if(block == NULL) {
    return NULL;
  }
  block->buddy_next = NULL;
  block->buddy_prev = NULL;
  block->arena = arena;
  block->buddy_order = order;
  block->buddy_flags = BUDDY_FLAG_FREE;
  block->buddy_offset = offset;
  block->mapping_base = NULL;
  block->mapping_size = 0;
  block->size = buddy_order_size(order) > sizeof(block_header_t)
                  ? buddy_order_size(order) - sizeof(block_header_t)
                  : 0u;
  block->flags = 0u;
  block->padding_reserved = 0u;
  block->padding_align = 0u;
  return block;
}

static void buddy_freelist_push(arena_header_t* arena, block_header_t* block) {
  if(block == NULL || arena == NULL || !buddy_order_valid(block->buddy_order)) {
    return;
  }

  size_t index = buddy_order_index(block->buddy_order);
  block->buddy_flags &= (uint32_t)~BUDDY_FLAG_USED;
  block->buddy_flags |= BUDDY_FLAG_FREE;
  block->buddy_prev = NULL;
  block->buddy_next = arena->buddy_freelists[index];
  if(block->buddy_next != NULL) {
    block->buddy_next->buddy_prev = block;
  }
  arena->buddy_freelists[index] = block;
}

static void buddy_freelist_remove(arena_header_t* arena, block_header_t* block) {
  if(block == NULL || arena == NULL || !buddy_order_valid(block->buddy_order)) {
    return;
  }

  if((block->buddy_flags & BUDDY_FLAG_FREE) == 0u) {
    return;
  }

  size_t index = buddy_order_index(block->buddy_order);
  if(block->buddy_prev != NULL) {
    block->buddy_prev->buddy_next = block->buddy_next;
  } else if(arena->buddy_freelists[index] == block) {
    arena->buddy_freelists[index] = block->buddy_next;
  }

  if(block->buddy_next != NULL) {
    block->buddy_next->buddy_prev = block->buddy_prev;
  }

  block->buddy_next = NULL;
  block->buddy_prev = NULL;
  block->buddy_flags &= (uint32_t)~BUDDY_FLAG_FREE;
  block->buddy_flags |= BUDDY_FLAG_USED;
}

static block_header_t* buddy_freelist_pop(arena_header_t* arena, uint32_t order) {
  if(arena == NULL || !buddy_order_valid(order)) {
    return NULL;
  }

  size_t index = buddy_order_index(order);
  block_header_t* block = arena->buddy_freelists[index];
  if(block != NULL) {
    buddy_freelist_remove(arena, block);
  }
  return block;
}

static block_header_t* buddy_freelist_find(arena_header_t* arena,
                                           uint32_t order,
                                           uintptr_t offset) {
  if(arena == NULL || !buddy_order_valid(order)) {
    return NULL;
  }

  size_t index = buddy_order_index(order);
  for(block_header_t* node = arena->buddy_freelists[index]; node != NULL; node = node->buddy_next) {
    if(node->buddy_offset == offset) {
      return node;
    }
  }
  return NULL;
}

static block_header_t* buddy_split_to_order(block_header_t* block, uint32_t target_order) {
  if(block == NULL || block->arena == NULL || !buddy_order_valid(target_order)) {
    return NULL;
  }

  if(block->buddy_order < target_order) {
    return NULL;
  }

  arena_header_t* arena = block->arena;

  while(block->buddy_order > target_order) {
    uint32_t new_order = block->buddy_order - 1u;
    size_t half_size = buddy_order_size(new_order);
    uintptr_t right_offset = block->buddy_offset + half_size;

    block_header_t* right = buddy_materialize_block(arena, right_offset, new_order);
    if(right == NULL) {
      break;
    }
    buddy_freelist_push(arena, right);

    block->buddy_order = new_order;
    block->size = buddy_order_size(new_order) - sizeof(block_header_t);
  }

  block->buddy_flags = BUDDY_FLAG_USED;
  block->buddy_next = NULL;
  block->buddy_prev = NULL;
  return block;
}

static block_header_t* buddy_coalesce_block(block_header_t* block) {
  if(block == NULL || block->arena == NULL) {
    return NULL;
  }

  arena_header_t* arena = block->arena;
  block->buddy_flags = BUDDY_FLAG_FREE;
  block->buddy_next = NULL;
  block->buddy_prev = NULL;

  while(block->buddy_order < BUDDY_MAX_ORDER) {
    size_t size = buddy_order_size(block->buddy_order);
    uintptr_t buddy_offset = block->buddy_offset ^ size;
    if(buddy_offset >= arena->buddy_size) {
      break;
    }
    block_header_t* buddy = buddy_freelist_find(arena, block->buddy_order, buddy_offset);
    if(buddy == NULL) {
      break;
    }

    buddy_freelist_remove(arena, buddy);

    if(buddy->buddy_offset < block->buddy_offset) {
      block = buddy;
    }

    block->buddy_offset &= ~(size);
    block->buddy_order += 1u;
    block->size = buddy_order_size(block->buddy_order) - sizeof(block_header_t);
    block->buddy_flags = BUDDY_FLAG_FREE;
    block->buddy_next = NULL;
    block->buddy_prev = NULL;
  }

  buddy_freelist_push(arena, block);
  return block;
}


#ifdef MENIOS_HOST_TEST

void __menios_allocator_reset(void) {
  allocator_lock_guard();
  arena_header_t* arena = arena_list_head;
  while(arena != NULL) {
    arena_header_t* next = arena->next;
    if(arena->mapping_base != NULL && arena->size != 0) {
      munmap(arena->mapping_base, arena->size);
    }
    arena = next;
  }

  arena_list_head = NULL;
  allocator_unlock_guard();
}

int __menios_allocator_grow_heap_for_test(size_t size) {
  allocator_lock_guard();
  int rc = grow_heap(size);
#ifdef MENIOS_HOST_TEST
  if(rc != 0) {
    static const char prefix[] = "grow_heap failed errno=";
    char number[32];
    (void)itoa(errno, number, 10);
    static const char suffix[] = "\n";
    (void)write(STDERR_FILENO, prefix, sizeof(prefix) - 1u);
    (void)write(STDERR_FILENO, number, strlen(number));
    (void)write(STDERR_FILENO, suffix, sizeof(suffix) - 1u);
  }
#endif
  allocator_unlock_guard();
  return rc;
}

block_header_t* __menios_buddy_debug_pop(uint32_t order) {
  block_header_t* result = NULL;
  allocator_lock_guard();
  if(buddy_order_valid(order)) {
    for(arena_header_t* arena = arena_list_head; arena != NULL; arena = arena->next) {
      block_header_t* block = buddy_freelist_pop(arena, order);
      if(block != NULL) {
        result = block;
        break;
      }
    }
  }
  allocator_unlock_guard();
  return result;
}

void __menios_buddy_debug_push(block_header_t* block) {
  if(block == NULL || block->arena == NULL) {
    return;
  }
  allocator_lock_guard();
  buddy_freelist_push(block->arena, block);
  allocator_unlock_guard();
}

block_header_t* __menios_buddy_debug_split(block_header_t* block, uint32_t target_order) {
  block_header_t* result = NULL;
  allocator_lock_guard();
  result = buddy_split_to_order(block, target_order);
  allocator_unlock_guard();
  return result;
}

block_header_t* __menios_buddy_debug_coalesce(block_header_t* block) {
  block_header_t* result = NULL;
  allocator_lock_guard();
  result = buddy_coalesce_block(block);
  allocator_unlock_guard();
  return result;
}

uint32_t __menios_buddy_debug_order(const block_header_t* block) {
  return block != NULL ? block->buddy_order : 0u;
}

uintptr_t __menios_buddy_debug_offset(const block_header_t* block) {
  return block != NULL ? block->buddy_offset : 0u;
}

arena_header_t* __menios_buddy_debug_arena(const block_header_t* block) {
  return block != NULL ? block->arena : NULL;
}

size_t __menios_buddy_debug_freelist_length(uint32_t order) {
  if(!buddy_order_valid(order)) {
    return 0u;
  }

  size_t total = 0u;
  size_t index = buddy_order_index(order);
  for(arena_header_t* arena = arena_list_head; arena != NULL; arena = arena->next) {
    for(block_header_t* node = arena->buddy_freelists[index]; node != NULL; node = node->buddy_next) {
      ++total;
    }
  }
  return total;
}

#endif /* MENIOS_HOST_TEST */

static inline bool is_power_of_two(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

static inline size_t align_up(size_t value, size_t alignment) {
  return (value + (alignment - 1)) & ~(alignment - 1);
}

static inline void* block_payload(block_header_t* block) {
  return (void*)(block + 1);
}

static inline block_header_t* payload_to_block(void* ptr) {
  return ((block_header_t*)ptr) - 1;
}

static inline bool block_is_direct(const block_header_t* block) {
  return (block->flags & BLOCK_FLAG_DIRECT) != 0u;
}

static size_t default_page_size(void) {
  static size_t cached = 0;
  if(cached != 0) {
    return cached;
  }

#ifdef MENIOS_HOST_TEST
  cached = 4096u;
  return cached;
#else
  long result = __menios_syscall0(SYS_GETPAGESIZE);
  if(result > 0) {
    cached = (size_t)result;
  } else {
    cached = 4096u;
  }
  return cached;
#endif
}

static inline size_t buddy_block_size(uint32_t order) {
  return buddy_order_size(order);
}

static inline size_t buddy_payload_capacity(uint32_t order) {
  size_t block_bytes = buddy_block_size(order);
  return block_bytes > sizeof(block_header_t)
           ? block_bytes - sizeof(block_header_t)
           : 0u;
}

static uint32_t buddy_order_for_size(size_t total) {
  uint32_t order = BUDDY_MIN_ORDER;
  size_t block_bytes = buddy_block_size(order);
  while(block_bytes < total && order < BUDDY_MAX_ORDER) {
    ++order;
    block_bytes = buddy_block_size(order);
  }
  if(block_bytes < total) {
    return (uint32_t)(BUDDY_MAX_ORDER + 1u);
  }
  return order;
}

static block_header_t* buddy_acquire_block(uint32_t order) {
  block_header_t* result = NULL;

  if(!buddy_order_valid(order)) {
    return NULL;
  }

  allocator_lock_guard();
  for(int attempt = 0; attempt < 2; ++attempt) {
    for(arena_header_t* arena = arena_list_head; arena != NULL; arena = arena->next) {
      for(uint32_t current = order; current <= BUDDY_MAX_ORDER; ++current) {
        block_header_t* candidate = buddy_freelist_pop(arena, current);
        if(candidate != NULL) {
          result = buddy_split_to_order(candidate, order);
          break;
        }
      }
      if(result != NULL) {
        break;
      }
    }

    if(result != NULL) {
      break;
    }

    if(grow_heap(buddy_block_size(order)) != 0) {
      break;
    }
  }
  allocator_unlock_guard();

  return result;
}

static block_header_t* buddy_allocate_block(size_t payload_size) {
  size_t total = payload_size + sizeof(block_header_t);
  if(total < payload_size) {
    errno = ENOMEM;
    return NULL;
  }

  uint32_t order = buddy_order_for_size(total);
  if(order > BUDDY_MAX_ORDER) {
    return NULL;
  }

  block_header_t* block = buddy_acquire_block(order);
  if(block == NULL) {
    errno = ENOMEM;
    return NULL;
  }

  block->buddy_flags = BUDDY_FLAG_USED;
  block->flags = BLOCK_FLAG_BUDDY;
  block->mapping_base = NULL;
  block->mapping_size = 0;
  block->size = buddy_payload_capacity(order);
  block->padding_reserved = 0u;
  block->padding_align = 0u;
  return block;
}

static void buddy_release_block(block_header_t* block) {
  if(block == NULL || block->arena == NULL) {
    return;
  }

  if(block->buddy_flags & BUDDY_FLAG_FREE) {
    static const char msg[] = "buddy_release_block: double free detected\n";
    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1u);
#ifdef MENIOS_HOST_TEST
    abort();
#endif
    errno = EINVAL;
    return;
  }

  block->flags &= (uint32_t)~BLOCK_FLAG_FREE;
  block->buddy_flags = BUDDY_FLAG_FREE;
  block->buddy_next = NULL;
  block->buddy_prev = NULL;
  block->size = buddy_payload_capacity(block->buddy_order);
  buddy_coalesce_block(block);
}

static int grow_heap(size_t size) {
  (void)size;

  const size_t page = default_page_size();
  size_t header_size = align_up(sizeof(arena_header_t), DEFAULT_ALIGNMENT);
  size_t buddy_offset = align_up(header_size, page);
  size_t buddy_bytes = ARENA_INITIAL_SIZE;
  size_t mapping_size = align_up(buddy_offset + buddy_bytes, page);

  void* mapping = mmap(NULL,
                       mapping_size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1,
                       0);
  if(mapping == MAP_FAILED) {
    errno = ENOMEM;
    return -1;
  }

  arena_header_t* arena = (arena_header_t*)mapping;
  arena->mapping_base = mapping;
  arena->size = mapping_size;
  arena->buddy_base = (uint8_t*)mapping + buddy_offset;
  arena->buddy_size = buddy_bytes;
  arena->prev = NULL;
  arena->next = arena_list_head;
  if(arena_list_head != NULL) {
    arena_list_head->prev = arena;
  }
  arena_list_head = arena;

  for(size_t i = 0; i < BUDDY_ORDER_COUNT; ++i) {
    arena->buddy_freelists[i] = NULL;
  }

  block_header_t* root = buddy_materialize_block(arena, 0u, BUDDY_MAX_ORDER);
  if(root == NULL) {
    munmap(mapping, mapping_size);
    errno = ENOMEM;
    return -1;
  }

  buddy_freelist_push(arena, root);
  return 0;
}

static void* allocate_direct(size_t size, size_t alignment) {
  if(!is_power_of_two(alignment) || alignment < sizeof(void*)) {
    alignment = DEFAULT_ALIGNMENT;
  }

  size_t padded = align_up(size, alignment);
  size_t extra = alignment + sizeof(block_header_t);
  if(padded > SIZE_MAX - extra) {
    errno = ENOMEM;
    return NULL;
  }

  size_t total = padded + extra;
  void* mapping = mmap(NULL,
                       total,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1,
                       0);
  if(mapping == MAP_FAILED) {
    errno = ENOMEM;
    return NULL;
  }

  uintptr_t base = (uintptr_t)mapping + sizeof(block_header_t);
  uintptr_t aligned_addr = align_up(base, alignment);
  block_header_t* header = (block_header_t*)(aligned_addr - sizeof(block_header_t));
  header->buddy_next = NULL;
  header->buddy_prev = NULL;
  header->arena = NULL;
  header->buddy_order = 0u;
  header->buddy_flags = 0u;
  header->buddy_offset = 0u;
  header->mapping_base = mapping;
  header->mapping_size = total;
  header->size = padded;
  header->flags = BLOCK_FLAG_DIRECT;
  header->padding_reserved = 0u;
  header->padding_align = 0u;
  direct_allocation_count++;
  direct_total_bytes += total;
  return block_payload(header);
}



void* malloc(size_t size) {
  if(size == 0) {
    return NULL;
  }

  size_t aligned = align_up(size, DEFAULT_ALIGNMENT);
  if(aligned < size) {
    errno = ENOMEM;
    return NULL;
  }

  size_t total = aligned + sizeof(block_header_t);
  if(total < aligned) {
    errno = ENOMEM;
    return NULL;
  }

  if(total > buddy_block_size(BUDDY_MAX_ORDER)) {
    return allocate_direct(aligned, DEFAULT_ALIGNMENT);
  }

  block_header_t* block = buddy_allocate_block(aligned);
  if(block == NULL) {
    return NULL;
  }

  return block_payload(block);
}

void free(void* ptr) {
  if(ptr == NULL) {
    return;
  }

  block_header_t* block = payload_to_block(ptr);
  if(block_is_direct(block)) {
    if(block->mapping_base != NULL && block->mapping_size != 0) {
      if(direct_allocation_count > 0) {
        direct_allocation_count--;
      }
      if(direct_total_bytes >= block->mapping_size) {
        direct_total_bytes -= block->mapping_size;
      } else {
        direct_total_bytes = 0;
      }
      (void)munmap(block->mapping_base, block->mapping_size);
    }
    return;
  }

  buddy_release_block(block);
}

void* calloc(size_t nmemb, size_t size) {
  if(nmemb == 0 || size == 0) {
    return malloc(0);
  }

  if(size != 0 && nmemb > SIZE_MAX / size) {
    errno = ENOMEM;
    return NULL;
  }

  size_t total = nmemb * size;
  void* ptr = malloc(total);
  if(ptr != NULL) {
    memset(ptr, 0, total);
  }
  return ptr;
}

void* realloc(void* ptr, size_t size) {
  if(ptr == NULL) {
    return malloc(size);
  }

  if(size == 0) {
    free(ptr);
    return NULL;
  }

  block_header_t* block = payload_to_block(ptr);
  size_t aligned = align_up(size, DEFAULT_ALIGNMENT);
  if(aligned < size) {
    errno = ENOMEM;
    return NULL;
  }

  if(block_is_direct(block)) {
    if(block->size >= aligned) {
      return ptr;
    }
  } else if(block->buddy_flags == BUDDY_FLAG_USED && block->size >= aligned) {
    return ptr;
  }

  size_t total = aligned + sizeof(block_header_t);
  void* replacement;
  if(total > buddy_block_size(BUDDY_MAX_ORDER) || block_is_direct(block)) {
    replacement = allocate_direct(aligned, DEFAULT_ALIGNMENT);
  } else {
    block_header_t* new_block = buddy_allocate_block(aligned);
    replacement = new_block ? block_payload(new_block) : NULL;
  }

  if(replacement == NULL) {
    errno = ENOMEM;
    return NULL;
  }

  size_t copy = block->size < size ? block->size : size;
  memcpy(replacement, ptr, copy);
  free(ptr);
  return replacement;
}

void* reallocarray(void* ptr, size_t nmemb, size_t size) {
  if(nmemb == 0 || size == 0) {
    free(ptr);
    return NULL;
  }

  if(size != 0 && nmemb > SIZE_MAX / size) {
    errno = ENOMEM;
    return NULL;
  }

  return realloc(ptr, nmemb * size);
}


size_t malloc_usable_size(void* ptr) {
  if(ptr == NULL) {
    return 0;
  }

  block_header_t* block = payload_to_block(ptr);
  return block->size;
}

int menios_malloc_stats(menios_malloc_stats_t* stats) {
  if(stats == NULL) {
    errno = EINVAL;
    return -1;
  }

  menios_malloc_stats_t snapshot = {0};

  for(arena_header_t* arena = arena_list_head; arena != NULL; arena = arena->next) {
    snapshot.arena_count++;
    snapshot.arena_payload_bytes += arena->buddy_size;
    for(uint32_t order = BUDDY_MIN_ORDER; order <= BUDDY_MAX_ORDER; ++order) {
      size_t index = buddy_order_index(order);
      for(block_header_t* node = arena->buddy_freelists[index]; node != NULL; node = node->buddy_next) {
        snapshot.buddy_free_blocks++;
        snapshot.buddy_free_payload_bytes += node->size;
      }
    }
  }

  snapshot.direct_allocations = direct_allocation_count;
  snapshot.direct_bytes = direct_total_bytes;

  *stats = snapshot;
  return 0;
}

void* aligned_alloc(size_t alignment, size_t size) {
  if(alignment == 0 || (alignment & (alignment - 1)) != 0) {
    errno = EINVAL;
    return NULL;
  }

  if(size % alignment != 0) {
    errno = EINVAL;
    return NULL;
  }

  if(alignment <= DEFAULT_ALIGNMENT) {
    return malloc(size);
  }

  return allocate_direct(size, alignment);
}

int posix_memalign(void** memptr, size_t alignment, size_t size) {
  if(memptr == NULL) {
    return EINVAL;
  }

  if(alignment == 0 || (alignment & (alignment - 1)) != 0 || alignment % sizeof(void*) != 0) {
    *memptr = NULL;
    return EINVAL;
  }

  void* ptr = allocate_direct(size, alignment);
  if(ptr == NULL) {
    int err = errno != 0 ? errno : ENOMEM;
    *memptr = NULL;
    return err;
  }

  *memptr = ptr;
  return 0;
}

void* memalign(size_t alignment, size_t size) {
  if(alignment == 0 || (alignment & (alignment - 1)) != 0) {
    errno = EINVAL;
    return NULL;
  }

  return allocate_direct(size, alignment);
}

void* valloc(size_t size) {
  size_t page = default_page_size();
  return allocate_direct(size == 0 ? page : size, page);
}

void* pvalloc(size_t size) {
  size_t page = default_page_size();
  if(page == 0) {
    errno = ENOMEM;
    return NULL;
  }

  if(size > SIZE_MAX - (page - 1)) {
    errno = ENOMEM;
    return NULL;
  }

  size_t rounded = size == 0 ? page : align_up(size, page);
  return allocate_direct(rounded, page);
}

static int digit_from_char(char ch) {
  if(ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if(ch >= 'a' && ch <= 'z') {
    return 10 + (ch - 'a');
  }
  if(ch >= 'A' && ch <= 'Z') {
    return 10 + (ch - 'A');
  }
  return -1;
}

long strtol(const char* nptr, char** endptr, int base) {
  const char* cursor = nptr;
  bool negative = false;
  bool any = false;
  bool overflow = false;
  unsigned long long acc = 0;

  if(nptr == NULL) {
    if(endptr != NULL) {
      *endptr = NULL;
    }
    errno = EINVAL;
    return 0;
  }

  while(*cursor != '\0' && isspace((unsigned char)*cursor)) {
    cursor++;
  }

  if(*cursor == '+' || *cursor == '-') {
    negative = (*cursor == '-');
    cursor++;
  }

  if(base == 0) {
    if(cursor[0] == '0') {
      if(cursor[1] == 'x' || cursor[1] == 'X') {
        base = 16;
        cursor += 2;
      } else {
        base = 8;
      }
    } else {
      base = 10;
    }
  } else if(base == 16 && cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
    cursor += 2;
  }

  if(base < 2 || base > 36) {
    if(endptr != NULL) {
      *endptr = (char*)nptr;
    }
    errno = EINVAL;
    return 0;
  }

  unsigned long long limit = negative ? ((unsigned long long)LONG_MAX + 1ULL) : (unsigned long long)LONG_MAX;
  unsigned long long cutoff = limit / (unsigned long long)base;
  unsigned int cutlim = (unsigned int)(limit % (unsigned long long)base);

  for(; *cursor != '\0'; cursor++) {
    int digit = digit_from_char(*cursor);
    if(digit < 0 || digit >= base) {
      break;
    }

    any = true;

    if(overflow) {
      continue;
    }

    if(acc > cutoff || (acc == cutoff && (unsigned int)digit > cutlim)) {
      overflow = true;
      acc = limit;
      continue;
    }

    acc = acc * (unsigned long long)base + (unsigned long long)digit;
  }

  if(endptr != NULL) {
    *endptr = any ? (char*)cursor : (char*)nptr;
  }

  if(!any) {
    errno = 0;
    return 0;
  }

  if(overflow) {
    errno = ERANGE;
    return negative ? LONG_MIN : LONG_MAX;
  }

  errno = 0;
  if(negative) {
    if(acc == ((unsigned long long)LONG_MAX + 1ULL)) {
      return LONG_MIN;
    }
    return -(long)acc;
  }

  return (long)acc;
}

#ifndef MENIOS_HOST_TEST
void _exit(int status) {
  __menios_syscall1(SYS_EXIT, (long)status);
  for(;;) {
    asm volatile("hlt");
  }
}

void exit(int status) {
  __menios_fini_libc(status);
  _exit(status);
}
#endif
#include <threads.h>

static mtx_t allocator_lock;
static once_flag allocator_once = ONCE_FLAG_INIT;

static void allocator_initialize_lock(void) {
  mtx_init(&allocator_lock, mtx_plain);
}

static inline void allocator_lock_guard(void) {
  call_once(&allocator_once, allocator_initialize_lock);
  mtx_lock(&allocator_lock);
}

static inline void allocator_unlock_guard(void) {
  mtx_unlock(&allocator_lock);
}
