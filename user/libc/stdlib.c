#include <ctype.h>
#include <limits.h>
#include <float.h>
#include <menios/syscall.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef MENIOS_HOST_TEST
typedef int wchar_t;
#endif
#include <stdatomic.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <unistd.h>

#ifndef MENIOS_HOST_TEST
#include <stdio.h>
#endif

static atomic_flag allocator_lock = ATOMIC_FLAG_INIT;

_Static_assert(sizeof(void*) == 8, "menios libc expects 64-bit pointers");

#define LCG_MULTIPLIER 1103515245u
#define LCG_INCREMENT  12345u

static atomic_uint rand_state = ATOMIC_VAR_INIT(1u);

#define ATEXIT_MAX_HANDLERS 32
static void (*atexit_handlers[ATEXIT_MAX_HANDLERS])(void) = {0};
static size_t atexit_handler_count = 0;

int rand(void) {
  unsigned int expected = atomic_load_explicit(&rand_state, memory_order_relaxed);
  unsigned int desired;
  do {
    desired = expected * LCG_MULTIPLIER + LCG_INCREMENT;
  } while(!atomic_compare_exchange_weak_explicit(&rand_state,
                                                 &expected,
                                                 desired,
                                                 memory_order_relaxed,
                                                 memory_order_relaxed));

  return (int)((desired >> 1u) & RAND_MAX);
}

void srand(unsigned int seed) {
  if(seed == 0u) {
    seed = 1u;
  }
  atomic_store_explicit(&rand_state, seed, memory_order_relaxed);
}

typedef struct {
  const char* end;
  double value;
  bool any;
  bool overflow;
  bool underflow;
} strto_parse_result_t;

static inline int ascii_tolower(int ch) {
  return tolower((unsigned char)ch);
}

static int hex_value(int ch) {
  if(ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if(ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if(ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

#if defined(__GNUC__) && !defined(__SSE2__)
#define MENIOS_FLOAT_PARSER_ATTR __attribute__((target("sse2")))
#else
#define MENIOS_FLOAT_PARSER_ATTR
#endif

static MENIOS_FLOAT_PARSER_ATTR strto_parse_result_t parse_decimal_number(const char* original,
                                                 const char* start,
                                                 int sign);
static MENIOS_FLOAT_PARSER_ATTR strto_parse_result_t parse_hex_number(const char* original,
                                             const char* digit_start,
                                             int sign);
static MENIOS_FLOAT_PARSER_ATTR strto_parse_result_t parse_floating_number(const char* nptr);

#ifndef MENIOS_HOST_TEST
static size_t debug_append_str(char* buffer, size_t pos, size_t capacity, const char* text) {
  while(text != NULL && *text != '\0' && pos < capacity) {
    buffer[pos++] = *text++;
  }
  return pos;
}

static size_t debug_append_hex(char* buffer, size_t pos, size_t capacity, uint64_t value) {
  static const char hex_digits[] = "0123456789abcdef";
  char tmp[16];
  size_t idx = 0u;

  if(value == 0) {
    tmp[idx++] = '0';
  } else {
    while(value != 0 && idx < sizeof(tmp)) {
      tmp[idx++] = hex_digits[value & 0xFu];
      value >>= 4u;
    }
  }

  while(idx > 0u && pos < capacity) {
    buffer[pos++] = tmp[--idx];
  }

  return pos;
}
#endif

static inline void allocator_lock_guard(void) {
  while(atomic_flag_test_and_set_explicit(&allocator_lock, memory_order_acquire)) {
#ifndef MENIOS_HOST_TEST
    __asm__ __volatile__("pause");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
  }
}

static inline void allocator_unlock_guard(void) {
  atomic_flag_clear_explicit(&allocator_lock, memory_order_release);
}

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

#ifdef MENIOS_HOST_TEST
#define DIRECT_ALLOCATION_THRESHOLD_BYTES (16u * 1024u * 1024u)
#if defined(__linux__)
#define HOST_MAP_SHARED     0x0001
#define HOST_MAP_PRIVATE    0x0002
#define HOST_MAP_ANONYMOUS  0x0020
#elif defined(__APPLE__)
#define HOST_MAP_SHARED     0x0001
#define HOST_MAP_PRIVATE    0x0002
#define HOST_MAP_ANONYMOUS  0x1000
#else
#define HOST_MAP_SHARED     0x0001
#define HOST_MAP_PRIVATE    0x0002
#define HOST_MAP_ANONYMOUS  0x1000
#endif
#else
#define DIRECT_ALLOCATION_THRESHOLD_BYTES ((size_t)1u << BUDDY_MAX_ORDER)
#endif

#ifdef MENIOS_HOST_TEST
static int host_translate_mmap_flags(int flags) {
  int host = 0;
  if(flags & MAP_SHARED) {
    host |= HOST_MAP_SHARED;
  }
  if(flags & MAP_PRIVATE) {
    host |= HOST_MAP_PRIVATE;
  }
  if(flags & MAP_ANONYMOUS) {
    host |= HOST_MAP_ANONYMOUS;
  }
  return host;
}
#endif

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
  struct arena_header* buddy_order_next[BUDDY_ORDER_COUNT];
  struct arena_header* buddy_order_prev[BUDDY_ORDER_COUNT];
} arena_header_t;

_Static_assert((sizeof(block_header_t) % DEFAULT_ALIGNMENT) == 0,
               "block header must stay aligned");

static arena_header_t* arena_list_head = NULL;
static arena_header_t* arena_order_heads[BUDDY_ORDER_COUNT] = {0};
static size_t direct_allocation_count = 0;
static size_t direct_total_bytes = 0;
static size_t double_free_attempts = 0;

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

static void buddy_order_list_insert(arena_header_t* arena, size_t index) {
  arena_header_t* head = arena_order_heads[index];
  arena->buddy_order_prev[index] = NULL;
  arena->buddy_order_next[index] = head;
  if(head != NULL) {
    head->buddy_order_prev[index] = arena;
  }
  arena_order_heads[index] = arena;
}

static void buddy_order_list_remove(arena_header_t* arena, size_t index) {
  arena_header_t* prev = arena->buddy_order_prev[index];
  arena_header_t* next = arena->buddy_order_next[index];
  if(prev != NULL) {
    prev->buddy_order_next[index] = next;
  } else if(arena_order_heads[index] == arena) {
    arena_order_heads[index] = next;
  }
  if(next != NULL) {
    next->buddy_order_prev[index] = prev;
  }
  arena->buddy_order_prev[index] = NULL;
  arena->buddy_order_next[index] = NULL;
}

static void buddy_freelist_push(arena_header_t* arena, block_header_t* block) {
  if(block == NULL || arena == NULL || !buddy_order_valid(block->buddy_order)) {
    return;
  }

  size_t index = buddy_order_index(block->buddy_order);
  bool was_empty = (arena->buddy_freelists[index] == NULL);
  block->buddy_flags &= (uint32_t)~BUDDY_FLAG_USED;
  block->buddy_flags |= BUDDY_FLAG_FREE;
  block->buddy_prev = NULL;
  block->buddy_next = arena->buddy_freelists[index];
  if(block->buddy_next != NULL) {
    block->buddy_next->buddy_prev = block;
  }
  arena->buddy_freelists[index] = block;
  if(was_empty) {
    buddy_order_list_insert(arena, index);
  }
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

  if(arena->buddy_freelists[index] == NULL) {
    buddy_order_list_remove(arena, index);
  }
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

static bool buddy_debug_poison_after_remove = false;
static bool buddy_debug_abort_on_double_free = true;

#ifdef MENIOS_HOST_TEST
void __menios_buddy_debug_poison_after_remove(bool enable) {
  buddy_debug_poison_after_remove = enable;
}

void __menios_buddy_debug_abort_on_double_free(bool enable) {
  buddy_debug_abort_on_double_free = enable;
}
#endif

static block_header_t* buddy_coalesce_block(block_header_t* block) {
  if(block == NULL || block->arena == NULL) {
    return NULL;
  }

  arena_header_t* arena = block->arena;
  block->buddy_flags = BUDDY_FLAG_FREE;
  block->buddy_next = NULL;
  block->buddy_prev = NULL;

  while(block->buddy_order < BUDDY_MAX_ORDER) {
    uint32_t current_order = block->buddy_order;
    size_t span = buddy_order_size(current_order);
    uintptr_t buddy_offset = block->buddy_offset ^ span;
    if(buddy_offset >= arena->buddy_size) {
      break;
    }
    block_header_t* buddy = buddy_block_from_offset(arena, buddy_offset);
    if(buddy == NULL || buddy->buddy_order != current_order || (buddy->buddy_flags & BUDDY_FLAG_FREE) == 0u) {
      break;
    }

    buddy_freelist_remove(arena, buddy);

    if(buddy_debug_poison_after_remove) {
      buddy->buddy_offset = UINTPTR_MAX;
      buddy->buddy_order = BUDDY_MIN_ORDER;
    }

    uintptr_t combined_offset = block->buddy_offset < buddy_offset ? block->buddy_offset : buddy_offset;
    uint32_t merged_order = current_order + 1u;

    block_header_t* merged = buddy_materialize_block(arena, combined_offset, merged_order);
    if(merged == NULL) {
      buddy_freelist_push(arena, buddy);
      break;
    }

    block = merged;
  }

  buddy_freelist_push(arena, block);
  return block;
}


#ifdef MENIOS_HOST_TEST

void __menios_allocator_reset(void) {
  allocator_lock_guard();
#if !defined(__linux__)
  arena_header_t* arena = arena_list_head;
  while(arena != NULL) {
    arena_header_t* next = arena->next;
    if(arena->mapping_base != NULL && arena->size != 0) {
      munmap(arena->mapping_base, arena->size);
    }
    arena = next;
  }
#endif

  arena_list_head = NULL;
  for(size_t i = 0; i < BUDDY_ORDER_COUNT; ++i) {
    arena_order_heads[i] = NULL;
  }
  allocator_unlock_guard();
}

int __menios_allocator_grow_heap_for_test(size_t size) {
  allocator_lock_guard();
  int rc = grow_heap(size);
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

static bool align_up_checked_size(size_t value, size_t alignment, size_t* out) {
  if(out == NULL || alignment == 0) {
    return false;
  }

  if((alignment & (alignment - 1)) != 0) {
    return false;
  }

  size_t mask = alignment - 1u;
  if(value > SIZE_MAX - mask) {
    return false;
  }

  *out = (value + mask) & ~mask;
  return true;
}

static bool align_up_checked_uintptr(uintptr_t value, size_t alignment, uintptr_t* out) {
  if(out == NULL || alignment == 0 || alignment > UINTPTR_MAX) {
    return false;
  }

  if((alignment & (alignment - 1)) != 0) {
    return false;
  }

  uintptr_t mask = (uintptr_t)alignment - 1u;
  if(value > UINTPTR_MAX - mask) {
    return false;
  }

  *out = (value + mask) & ~mask;
  return true;
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
  for(int attempt = 0; attempt < 2 && result == NULL; ++attempt) {
    for(uint32_t current = order; current <= BUDDY_MAX_ORDER && result == NULL; ++current) {
      size_t index = buddy_order_index(current);
      arena_header_t* arena = arena_order_heads[index];
      while(arena != NULL && result == NULL) {
        arena_header_t* next = arena->buddy_order_next[index];
        block_header_t* candidate = buddy_freelist_pop(arena, current);
        if(candidate != NULL) {
          result = buddy_split_to_order(candidate, order);
        }
        arena = next;
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
  allocator_lock_guard();
  if(block == NULL || block->arena == NULL) {
    allocator_unlock_guard();
    return;
  }

  if(block->buddy_flags & BUDDY_FLAG_FREE) {
#ifndef MENIOS_HOST_TEST
    static const char msg[] = "buddy_release_block: double free detected\n";
    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1u);
#endif
    ++double_free_attempts;
#ifdef MENIOS_HOST_TEST
    if(buddy_debug_abort_on_double_free) {
      abort();
    }
#endif
    errno = EINVAL;
    allocator_unlock_guard();
    return;
  }

  block->flags &= (uint32_t)~BLOCK_FLAG_FREE;
  block->buddy_flags = BUDDY_FLAG_FREE;
  block->buddy_next = NULL;
  block->buddy_prev = NULL;
  block->size = buddy_payload_capacity(block->buddy_order);
  buddy_coalesce_block(block);
  allocator_unlock_guard();
}

static int grow_heap(size_t size) {
  (void)size;

  const size_t page = default_page_size();
  size_t header_size = align_up(sizeof(arena_header_t), DEFAULT_ALIGNMENT);
  size_t buddy_offset = align_up(header_size, page);
  size_t buddy_bytes = ARENA_INITIAL_SIZE;
  size_t mapping_size = align_up(buddy_offset + buddy_bytes, page);

  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MENIOS_HOST_TEST
  mmap_flags = host_translate_mmap_flags(mmap_flags);
#endif

  void* mapping = mmap(NULL,
                       mapping_size,
                       PROT_READ | PROT_WRITE,
                       mmap_flags,
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

#ifndef MENIOS_HOST_TEST
  {
    char logbuf[160];
    size_t pos = 0u;
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), "grow_heap: mapping=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)mapping);
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), " buddy_base=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)arena->buddy_base);
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), " buddy_size=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)arena->buddy_size);
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), " header=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)(uintptr_t)arena);
    if(pos < sizeof(logbuf)) {
      logbuf[pos++] = '\n';
    }
    (void)write(STDERR_FILENO, logbuf, pos);
  }
#endif

  for(size_t i = 0; i < BUDDY_ORDER_COUNT; ++i) {
    arena->buddy_freelists[i] = NULL;
    arena->buddy_order_next[i] = NULL;
    arena->buddy_order_prev[i] = NULL;
  }

  block_header_t* root = buddy_materialize_block(arena, 0u, BUDDY_MAX_ORDER);
  if(root == NULL) {
    if(arena_list_head == arena) {
      arena_list_head = arena->next;
      if(arena_list_head != NULL) {
        arena_list_head->prev = NULL;
      }
    } else if(arena->prev != NULL) {
      arena->prev->next = arena->next;
      if(arena->next != NULL) {
        arena->next->prev = arena->prev;
      }
    }
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

  size_t padded = 0u;
  if(!align_up_checked_size(size, alignment, &padded)) {
    errno = ENOMEM;
    return NULL;
  }

  if(alignment > SIZE_MAX - sizeof(block_header_t)) {
    errno = ENOMEM;
    return NULL;
  }

  size_t extra = alignment + sizeof(block_header_t);
  if(padded > SIZE_MAX - extra) {
    errno = ENOMEM;
    return NULL;
  }

  size_t total = padded + extra;
  if(total > UINTPTR_MAX) {
    errno = ENOMEM;
    return NULL;
  }

  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MENIOS_HOST_TEST
  mmap_flags = host_translate_mmap_flags(mmap_flags);
#endif

  void* mapping = mmap(NULL,
                       total,
                       PROT_READ | PROT_WRITE,
                       mmap_flags,
                       -1,
                       0);
  if(mapping == MAP_FAILED) {
    errno = ENOMEM;
    return NULL;
  }

  uintptr_t mapping_start = (uintptr_t)mapping;
  if(mapping_start > UINTPTR_MAX - total) {
    munmap(mapping, total);
    errno = ENOMEM;
    return NULL;
  }

  uintptr_t mapping_end = mapping_start + total;

  if(sizeof(block_header_t) > mapping_end - mapping_start) {
    munmap(mapping, total);
    errno = ENOMEM;
    return NULL;
  }

  uintptr_t base = mapping_start + sizeof(block_header_t);
  uintptr_t aligned_addr = 0u;
  if(!align_up_checked_uintptr(base, alignment, &aligned_addr)) {
    munmap(mapping, total);
    errno = ENOMEM;
    return NULL;
  }

  if(aligned_addr > mapping_end) {
    munmap(mapping, total);
    errno = ENOMEM;
    return NULL;
  }

  if(padded != 0u) {
    uintptr_t required_end = aligned_addr + (uintptr_t)padded;
    if(required_end < aligned_addr || required_end > mapping_end) {
      munmap(mapping, total);
      errno = ENOMEM;
      return NULL;
    }
  }

  uintptr_t header_addr = aligned_addr - sizeof(block_header_t);
  if(header_addr < mapping_start) {
    munmap(mapping, total);
    errno = ENOMEM;
    return NULL;
  }

  block_header_t* header = (block_header_t*)header_addr;
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
  allocator_lock_guard();
  direct_allocation_count++;
  direct_total_bytes += total;
  allocator_unlock_guard();
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

  if(total > DIRECT_ALLOCATION_THRESHOLD_BYTES) {
    return allocate_direct(aligned, DEFAULT_ALIGNMENT);
  }

  block_header_t* block = buddy_allocate_block(aligned);
  if(block == NULL) {
    return NULL;
  }

#if defined(MENIOS_ENABLE_MALLOC_LOGS) && !defined(MENIOS_HOST_TEST)
  {
    char logbuf[128];
    size_t pos = 0u;
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), "malloc: size=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)size);
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), " aligned=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)aligned);
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), " ptr=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)block_payload(block));
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), " arena=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)block->arena);
    pos = debug_append_str(logbuf, pos, sizeof(logbuf), " order=0x");
    pos = debug_append_hex(logbuf, pos, sizeof(logbuf), (uint64_t)block->buddy_order);
    if(pos < sizeof(logbuf)) {
      logbuf[pos++] = '\n';
    }
    (void)write(STDERR_FILENO, logbuf, pos);
  }
#endif

  return block_payload(block);
}

void free(void* ptr) {
  if(ptr == NULL) {
    return;
  }

  block_header_t* block = payload_to_block(ptr);
  if(block_is_direct(block)) {
    if(block->mapping_base != NULL && block->mapping_size != 0) {
      allocator_lock_guard();
      if(direct_allocation_count > 0) {
        direct_allocation_count--;
      }
      if(direct_total_bytes >= block->mapping_size) {
        direct_total_bytes -= block->mapping_size;
      } else {
        direct_total_bytes = 0;
      }
      allocator_unlock_guard();
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

  allocator_lock_guard();
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
  snapshot.double_free_attempts = double_free_attempts;

  *stats = snapshot;
  allocator_unlock_guard();
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

static MENIOS_FLOAT_PARSER_ATTR strto_parse_result_t parse_decimal_number(const char* original,
                                                 const char* start,
                                                 int sign) {
  strto_parse_result_t result = { .end = original, .value = 0.0, .any = false, .overflow = false, .underflow = false };
  const char* p = start;
  double value = 0.0;
  bool overflow = false;
  bool underflow = false;
  bool any_digit = false;
  bool any_nonzero = false;

  while(isdigit((unsigned char)*p)) {
    int digit = *p - '0';
    if(digit != 0) {
      any_nonzero = true;
    }
    if(!overflow) {
      value = value * 10.0 + digit;
      if(!__builtin_isfinite(value) || fabs(value) > DBL_MAX) {
        overflow = true;
        value = DBL_MAX;
      }
    }
    any_digit = true;
    p++;
  }

  int frac_digits = 0;
  if(*p == '.') {
    p++;
    while(isdigit((unsigned char)*p)) {
      int digit = *p - '0';
      if(digit != 0) {
        any_nonzero = true;
      }
      if(!overflow) {
        value = value * 10.0 + digit;
        if(!__builtin_isfinite(value) || fabs(value) > DBL_MAX) {
          overflow = true;
          value = DBL_MAX;
        }
      }
      any_digit = true;
      frac_digits++;
      p++;
    }
  }

  if(!any_digit) {
    return result;
  }

  int exponent = 0;
  int exp_sign = 1;
  const char* exp_pos = p;
  if(*p == 'e' || *p == 'E') {
    p++;
    if(*p == '+' || *p == '-') {
      if(*p == '-') {
        exp_sign = -1;
      }
      p++;
    }
    const char* exp_digits = p;
    if(!isdigit((unsigned char)*p)) {
      p = exp_pos;
    } else {
      while(isdigit((unsigned char)*p)) {
        if(exponent < 1000000) {
          exponent = exponent * 10 + (*p - '0');
        }
        p++;
      }
      exponent *= exp_sign;
    }
  }

  double scaled = value;
  if(!overflow) {
    int total_exp = exponent - frac_digits;
    if(total_exp > 0) {
      for(int i = 0; i < total_exp; ++i) {
        scaled *= 10.0;
        if(!__builtin_isfinite(scaled) || fabs(scaled) > DBL_MAX) {
          overflow = true;
          scaled = HUGE_VAL;
          break;
        }
      }
    } else if(total_exp < 0) {
      for(int i = 0; i < -total_exp; ++i) {
        double prev = scaled;
        scaled /= 10.0;
        if(prev != 0.0 && scaled == 0.0) {
          underflow = true;
          break;
        }
      }
    }
  }

  if(overflow) {
    scaled = sign > 0 ? HUGE_VAL : -HUGE_VAL;
    underflow = false;
  } else {
    if(sign < 0) {
      scaled = -scaled;
    }
    if(underflow) {
      scaled = sign < 0 ? -0.0 : 0.0;
    }
  }

  if(!overflow && !underflow && scaled == 0.0 && any_nonzero && exponent < frac_digits) {
    underflow = true;
    scaled = sign < 0 ? -0.0 : 0.0;
  }

  result.any = true;
  result.value = scaled;
  result.overflow = overflow;
  result.underflow = underflow;
  result.end = p;
  return result;
}

static MENIOS_FLOAT_PARSER_ATTR strto_parse_result_t parse_hex_number(const char* original,
                                             const char* digit_start,
                                             int sign) {
  strto_parse_result_t result = { .end = original, .value = 0.0, .any = false, .overflow = false, .underflow = false };
  const char* p = digit_start;
  double value = 0.0;
  bool overflow = false;
  bool underflow = false;
  bool any_digit = false;
  bool any_nonzero = false;
  int frac_bits = 0;

  while(true) {
    int digit = hex_value(*p);
    if(digit < 0) {
      break;
    }
    any_digit = true;
    if(digit != 0) {
      any_nonzero = true;
    }
    if(!overflow) {
      value = value * 16.0 + digit;
      if(!__builtin_isfinite(value) || fabs(value) > DBL_MAX) {
        overflow = true;
        value = DBL_MAX;
      }
    }
    p++;
  }

  if(*p == '.') {
    p++;
    while(true) {
      int digit = hex_value(*p);
      if(digit < 0) {
        break;
      }
      any_digit = true;
      if(digit != 0) {
        any_nonzero = true;
      }
      if(!overflow) {
        value = value * 16.0 + digit;
        if(!__builtin_isfinite(value) || fabs(value) > DBL_MAX) {
          overflow = true;
          value = DBL_MAX;
        }
      }
      frac_bits += 4;
      p++;
    }
  }

  if(!any_digit) {
    return result;
  }

  int exp_val = 0;
  int exp_sign = 1;
  const char* exp_pos = p;
  if(*p == 'p' || *p == 'P') {
    p++;
    if(*p == '+' || *p == '-') {
      if(*p == '-') {
        exp_sign = -1;
      }
      p++;
    }
    const char* exp_digits = p;
    if(!isdigit((unsigned char)*p)) {
      p = exp_pos;
    } else {
      while(isdigit((unsigned char)*p)) {
        if(exp_val < 1000000) {
          exp_val = exp_val * 10 + (*p - '0');
        }
        p++;
      }
      exp_val *= exp_sign;
    }
  }

  double scaled = value;
  if(!overflow) {
    int total_exp = exp_val - frac_bits;
    if(value != 0.0 && total_exp != 0) {
      scaled = ldexp(value, total_exp);
      if(!__builtin_isfinite(scaled) || fabs(scaled) > DBL_MAX) {
        overflow = true;
        scaled = HUGE_VAL;
      } else if(scaled == 0.0 && any_nonzero) {
        underflow = true;
      }
    } else if(value == 0.0) {
      scaled = 0.0;
    }
  }

  if(overflow) {
    scaled = sign > 0 ? HUGE_VAL : -HUGE_VAL;
    underflow = false;
  } else {
    if(sign < 0) {
      scaled = -scaled;
    }
    if(underflow) {
      scaled = sign < 0 ? -0.0 : 0.0;
    }
  }

  result.any = true;
  result.value = scaled;
  result.overflow = overflow;
  result.underflow = underflow;
  result.end = p;
  return result;
}

static MENIOS_FLOAT_PARSER_ATTR strto_parse_result_t parse_floating_number(const char* nptr) {
  strto_parse_result_t result = { .end = nptr, .value = 0.0, .any = false, .overflow = false, .underflow = false };
  const char* p = nptr;
  while(isspace((unsigned char)*p)) {
    p++;
  }

  int sign = 1;
  if(*p == '+' || *p == '-') {
    if(*p == '-') {
      sign = -1;
    }
    p++;
  }

  if(strncasecmp(p, "inf", 3) == 0) {
    p += 3;
    if(strncasecmp(p, "inity", 5) == 0) {
      p += 5;
    }
    double val = sign < 0 ? -INFINITY : INFINITY;
    result.any = true;
    result.value = val;
    result.end = p;
    return result;
  }

  if(strncasecmp(p, "nan", 3) == 0) {
    p += 3;
    const char* payload_start = p;
    if(*p == '(') {
      p++;
      while(*p != '\0' && *p != ')') {
        p++;
      }
      if(*p == ')') {
        p++;
      } else {
        p = payload_start;
      }
    }
    double val = __builtin_nan("");
    if(sign < 0) {
      val = -val;
    }
    result.any = true;
    result.value = val;
    result.end = p;
    return result;
  }

  if(p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    return parse_hex_number(nptr, p + 2, sign);
  }

  return parse_decimal_number(nptr, p, sign);
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

int atoi(const char* nptr) {
  return (int)strtol(nptr, NULL, 10);
}

long atol(const char* nptr) {
  return strtol(nptr, NULL, 10);
}

MENIOS_FLOAT_PARSER_ATTR double strtod(const char* nptr, char** endptr) {
  strto_parse_result_t parsed = parse_floating_number(nptr);
  if(endptr != NULL) {
    *endptr = (char*)(parsed.any ? parsed.end : nptr);
  }

  if(!parsed.any) {
    errno = 0;
    return 0.0;
  }

  double base_value = parsed.value;
  double result = (double)base_value;
  int negative = signbit(base_value) ? 1 : 0;
  bool result_infinite = !__builtin_isfinite(result);
  bool base_finite = __builtin_isfinite(base_value);

  if(parsed.overflow || (result_infinite && base_finite)) {
    errno = ERANGE;
    return __builtin_copysign(HUGE_VAL, negative ? -1.0 : 1.0);
  }

  if(parsed.underflow || (result == 0.0 && base_value != 0.0)) {
    errno = ERANGE;
    return __builtin_copysign(0.0, negative ? -1.0 : 1.0);
  }

  errno = 0;
  return result;
}

MENIOS_FLOAT_PARSER_ATTR float strtof(const char* nptr, char** endptr) {
  strto_parse_result_t parsed = parse_floating_number(nptr);
  if(endptr != NULL) {
    *endptr = (char*)(parsed.any ? parsed.end : nptr);
  }

  if(!parsed.any) {
    errno = 0;
    return 0.0f;
  }

  double base_value = parsed.value;
  float result = (float)base_value;
  int negative = signbit(base_value) ? 1 : 0;
  bool result_infinite = !__builtin_isfinite(result);
  bool base_finite = __builtin_isfinite(base_value);

  if(parsed.overflow || (result_infinite && base_finite)) {
    errno = ERANGE;
    return __builtin_copysignf(HUGE_VALF, negative ? -1.0f : 1.0f);
  }

  if(parsed.underflow || (result == 0.0f && base_value != 0.0)) {
    errno = ERANGE;
    return __builtin_copysignf(0.0f, negative ? -1.0f : 1.0f);
  }

  errno = 0;
  return result;
}

MENIOS_FLOAT_PARSER_ATTR double atof(const char* nptr) {
  return strtod(nptr, NULL);
}

#undef MENIOS_FLOAT_PARSER_ATTR

size_t mbstowcs(wchar_t* dest, const char* src, size_t max) {
  if(src == NULL) {
    errno = EINVAL;
    return (size_t)-1;
  }

  size_t count = 0;
  if(dest == NULL || max == 0) {
    while(src[count] != '\0') {
      count++;
    }
    return count;
  }

  while(count < max && src[count] != '\0') {
    dest[count] = (unsigned char)src[count];
    count++;
  }

  if(count < max) {
    dest[count] = L'\0';
  } else {
    dest[max - 1] = L'\0';
  }

  return count;
}

size_t wcstombs(char* dest, const wchar_t* src, size_t max) {
  if(src == NULL) {
    errno = EINVAL;
    return (size_t)-1;
  }

  size_t count = 0;
  if(dest == NULL || max == 0) {
    while(src[count] != L'\0') {
      count++;
    }
    return count;
  }

  while(count < max && src[count] != L'\0') {
    wchar_t wc = src[count];
    if(wc > 0xff) {
      errno = EILSEQ;
      return (size_t)-1;
    }
    dest[count] = (char)wc;
    count++;
  }

  if(count < max) {
    dest[count] = '\0';
  } else {
    dest[max - 1] = '\0';
  }

  return count;
}

char* mktemp(char* templ) {
  if(templ == NULL) {
    errno = EINVAL;
    return NULL;
  }

  size_t len = strlen(templ);
  if(len < 6) {
    errno = EINVAL;
    if(len > 0) {
      templ[0] = '\0';
    }
    return templ;
  }

  char* pattern = templ + len - 6;
  if(strncmp(pattern, "XXXXXX", 6) != 0) {
    errno = EINVAL;
    templ[0] = '\0';
    return templ;
  }

  static unsigned long counter = 0;
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const size_t alpha_len = sizeof(alphabet) - 1;

  unsigned long value = counter++;
  for(int i = 0; i < 6; ++i) {
    pattern[i] = alphabet[value % alpha_len];
    value /= alpha_len;
  }

  errno = 0;
  return templ;
}

int mkstemp(char* templ) {
  if(templ == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(mktemp(templ) == NULL || templ[0] == '\0') {
    return -1;
  }

  int fd = open(templ, O_RDWR | O_CREAT | O_EXCL, 0600);
  if(fd < 0) {
    templ[0] = '\0';
    return -1;
  }

  return fd;
}

FILE* tmpfile(void) {
  char templ[] = "/tmp/meniosXXXXXX";
  int fd = mkstemp(templ);
  if(fd < 0) {
    return NULL;
  }

  (void)unlink(templ);

  FILE* stream = fdopen(fd, "w+b");
  if(stream == NULL) {
    int saved = errno;
    close(fd);
    errno = saved;
    return NULL;
  }

  return stream;
}

static void swap_elements(unsigned char* a, unsigned char* b, size_t size) {
  for(size_t i = 0; i < size; ++i) {
    unsigned char tmp = a[i];
    a[i] = b[i];
    b[i] = tmp;
  }
}

void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
  if(base == NULL || compar == NULL || size == 0) {
    return;
  }

  unsigned char* data = (unsigned char*)base;
  for(size_t i = 0; i < nmemb; ++i) {
    size_t min_index = i;
    for(size_t j = i + 1; j < nmemb; ++j) {
      unsigned char* elem = data + j * size;
      unsigned char* min_elem = data + min_index * size;
      if(compar(elem, min_elem) < 0) {
        min_index = j;
      }
    }
    if(min_index != i) {
      swap_elements(data + i * size, data + min_index * size, size);
    }
  }
}

__attribute__((weak)) void* bsearch(const void* key,
                                    const void* base,
                                    size_t nmemb,
                                    size_t size,
                                    int (*compar)(const void*, const void*)) {
  if(key == NULL || base == NULL || compar == NULL || size == 0) {
    return NULL;
  }

  size_t low = 0;
  size_t high = nmemb;
  const unsigned char* data = (const unsigned char*)base;

  while(low < high) {
    size_t mid = low + (high - low) / 2;
    const void* element = data + mid * size;
    int cmp = compar(key, element);
    if(cmp < 0) {
      high = mid;
    } else if(cmp > 0) {
      low = mid + 1;
    } else {
      return (void*)element;
    }
  }

  return NULL;
}

int abs(int value) {
  return (value < 0) ? -value : value;
}

long labs(long value) {
  return (value < 0) ? -value : value;
}

long long llabs(long long value) {
  return (value < 0) ? -value : value;
}

int system(const char* command) {
  (void)command;
  errno = ENOSYS;
  return -1;
}

int atexit(void (*func)(void)) {
  if(func == NULL) {
    errno = EINVAL;
    return -1;
  }
  if(atexit_handler_count >= ATEXIT_MAX_HANDLERS) {
    errno = ENOMEM;
    return -1;
  }
  atexit_handlers[atexit_handler_count++] = func;
  return 0;
}

void __menios_atexit_run(void) {
  while(atexit_handler_count > 0) {
    void (*handler)(void) = atexit_handlers[--atexit_handler_count];
    if(handler != NULL) {
      handler();
    }
  }
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
