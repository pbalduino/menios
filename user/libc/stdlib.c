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

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MENIOS_HOST_TEST
#include <menios/syscall_user.h>
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
#define MIN_SPLIT_SIZE      64u
#define ARENA_INITIAL_SIZE  (1u << 20)   /* 1 MiB */
#define ARENA_MAX_SIZE      (1u << 27)   /* 128 MiB */
#define BLOCK_FLAG_FREE     (1u << 0)
#define BLOCK_FLAG_DIRECT   (1u << 1)

struct arena_header;
typedef struct block_header {
  struct block_header* next;      /* neighbour inside arena */
  struct block_header* prev;
  struct block_header* free_next; /* intrusive freelist linkage */
  struct block_header* free_prev;
  struct arena_header* arena;     /* NULL for directly mapped blocks */
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
  size_t size;                    /* total bytes mapped for this arena */
  block_header_t* first_block;
} arena_header_t;

_Static_assert((sizeof(block_header_t) % DEFAULT_ALIGNMENT) == 0,
               "block header must stay aligned");

static arena_header_t* arena_list_head = NULL;
static block_header_t* free_list_head = NULL;
static size_t next_arena_size = ARENA_INITIAL_SIZE;

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

static inline bool block_is_free(const block_header_t* block) {
  return (block->flags & BLOCK_FLAG_FREE) != 0u;
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

static void free_list_remove(block_header_t* block) {
  if(!block_is_free(block)) {
    return;
  }

  if(block->free_prev != NULL) {
    block->free_prev->free_next = block->free_next;
  } else if(free_list_head == block) {
    free_list_head = block->free_next;
  }

  if(block->free_next != NULL) {
    block->free_next->free_prev = block->free_prev;
  }

  block->free_next = NULL;
  block->free_prev = NULL;
  block->flags &= (uint32_t)~BLOCK_FLAG_FREE;
}

static void free_list_push(block_header_t* block) {
  block->flags |= BLOCK_FLAG_FREE;
  block->free_prev = NULL;
  block->free_next = free_list_head;
  if(free_list_head != NULL) {
    free_list_head->free_prev = block;
  }
  free_list_head = block;
}

static block_header_t* coalesce_with_neighbours(block_header_t* block) {
  if(block->arena == NULL) {
    return block;
  }

  block_header_t* prev = block->prev;
  if(prev != NULL && block_is_free(prev) && prev->arena == block->arena) {
    free_list_remove(prev);
    prev->flags |= BLOCK_FLAG_FREE;
    if(block->arena->first_block == block) {
      block->arena->first_block = prev;
    }
    prev->size += sizeof(block_header_t) + block->size;
    prev->next = block->next;
    if(block->next != NULL) {
      block->next->prev = prev;
    }
    block = prev;
  }

  block_header_t* next = block->next;
  if(next != NULL && block_is_free(next) && next->arena == block->arena) {
    free_list_remove(next);
    block->flags |= BLOCK_FLAG_FREE;
    if(block->arena->first_block == next) {
      block->arena->first_block = block;
    }
    block->size += sizeof(block_header_t) + next->size;
    block->next = next->next;
    if(next->next != NULL) {
      next->next->prev = block;
    }
  }

  return block;
}

static block_header_t* split_block(block_header_t* block, size_t size) {
  size_t total = block->size;
  if(total < size + sizeof(block_header_t) + MIN_SPLIT_SIZE) {
    return block;
  }

  uint8_t* payload = (uint8_t*)block_payload(block);
  block_header_t* new_block = (block_header_t*)(payload + size);

  new_block->next = block->next;
  new_block->prev = block;
  new_block->free_next = NULL;
  new_block->free_prev = NULL;
  new_block->arena = block->arena;
  new_block->mapping_base = NULL;
  new_block->mapping_size = 0;
  new_block->size = total - size - sizeof(block_header_t);
  new_block->flags = BLOCK_FLAG_FREE;

  if(block->next != NULL) {
    block->next->prev = new_block;
  }
  block->next = new_block;
  block->size = size;

  free_list_push(new_block);
  return block;
}

static block_header_t* find_suitable_block(size_t size) {
  for(block_header_t* current = free_list_head; current != NULL; current = current->free_next) {
    if(current->size >= size) {
      return current;
    }
  }
  return NULL;
}

static size_t arena_overhead(void) {
  return align_up(sizeof(arena_header_t), DEFAULT_ALIGNMENT) + sizeof(block_header_t);
}

static int grow_heap(size_t size) {
  const size_t overhead = arena_overhead();
  const size_t page = default_page_size();
  size_t total_needed = overhead + size;

  if(total_needed < size || total_needed > SIZE_MAX - DEFAULT_ALIGNMENT) {
    errno = ENOMEM;
    return -1;
  }

  size_t arena_size = next_arena_size;
  if(arena_size < total_needed) {
    while(arena_size < total_needed && arena_size < ARENA_MAX_SIZE) {
      arena_size <<= 1;
    }
  }

  if(arena_size < total_needed) {
    arena_size = align_up(total_needed, page);
  }

  arena_size = align_up(arena_size, page);

  void* mapping = mmap(NULL, arena_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(mapping == MAP_FAILED) {
    errno = ENOMEM;
    return -1;
  }

  arena_header_t* arena = (arena_header_t*)mapping;
  arena->size = arena_size;
  arena->prev = NULL;
  arena->next = arena_list_head;
  if(arena_list_head != NULL) {
    arena_list_head->prev = arena;
  }
  arena_list_head = arena;

  size_t header_size = align_up(sizeof(arena_header_t), DEFAULT_ALIGNMENT);
  block_header_t* block = (block_header_t*)((uint8_t*)mapping + header_size);
  block->next = NULL;
  block->prev = NULL;
  block->free_next = NULL;
  block->free_prev = NULL;
  block->arena = arena;
  block->mapping_base = NULL;
  block->mapping_size = 0;
  block->size = arena_size - header_size - sizeof(block_header_t);
  block->flags = BLOCK_FLAG_FREE;
  arena->first_block = block;

  free_list_push(block);

  if(arena_size < ARENA_MAX_SIZE) {
    size_t prospective = arena_size << 1;
    if(prospective > ARENA_MAX_SIZE) {
      prospective = ARENA_MAX_SIZE;
    }
    next_arena_size = prospective;
  } else {
    next_arena_size = ARENA_MAX_SIZE;
  }

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
  uintptr_t aligned = align_up(base, alignment);
  block_header_t* header = (block_header_t*)(aligned - sizeof(block_header_t));
  header->next = NULL;
  header->prev = NULL;
  header->free_next = NULL;
  header->free_prev = NULL;
  header->arena = NULL;
  header->mapping_base = mapping;
  header->mapping_size = total;
  header->size = padded;
  header->flags = BLOCK_FLAG_DIRECT;

  return block_payload(header);
}

static block_header_t* allocate_block(size_t size) {
  block_header_t* block = find_suitable_block(size);
  if(block == NULL) {
    if(grow_heap(size) != 0) {
      return NULL;
    }
    block = find_suitable_block(size);
    if(block == NULL) {
      return NULL;
    }
  }

  free_list_remove(block);
  block = split_block(block, size);
  block->flags &= (uint32_t)~BLOCK_FLAG_FREE;
  block->free_next = NULL;
  block->free_prev = NULL;
  return block;
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

  const size_t overhead = arena_overhead();
  if(aligned > ARENA_MAX_SIZE - overhead) {
    return allocate_direct(aligned, DEFAULT_ALIGNMENT);
  }

  block_header_t* block = allocate_block(aligned);
  if(block == NULL) {
    errno = ENOMEM;
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
      (void)munmap(block->mapping_base, block->mapping_size);
    }
    return;
  }

  if(block_is_free(block)) {
    return; /* ignore obvious double free */
  }

  block->flags |= BLOCK_FLAG_FREE;
  block->free_next = NULL;
  block->free_prev = NULL;

  block = coalesce_with_neighbours(block);
  free_list_push(block);
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

static void* realloc_grow_in_place(block_header_t* block, size_t size) {
  if(block_is_direct(block)) {
    return NULL;
  }

  block_header_t* next = block->next;
  if(next != NULL && block_is_free(next) && next->arena == block->arena) {
    size_t merged = block->size + sizeof(block_header_t) + next->size;
    if(merged >= size) {
      free_list_remove(next);
      block->size = merged;
      block->next = next->next;
      if(block->next != NULL) {
        block->next->prev = block;
      }
      block = split_block(block, size);
      block->flags &= (uint32_t)~BLOCK_FLAG_FREE;
      block->free_next = NULL;
      block->free_prev = NULL;
      return block_payload(block);
    }
  }

  return NULL;
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

  if(block->size >= aligned) {
    return ptr;
  }

  if(!block_is_direct(block)) {
    void* grown = realloc_grow_in_place(block, aligned);
    if(grown != NULL) {
      return grown;
    }
  }

  void* replacement;
  const size_t overhead = arena_overhead();
  if(aligned > ARENA_MAX_SIZE - overhead || block_is_direct(block)) {
    replacement = allocate_direct(aligned, DEFAULT_ALIGNMENT);
  } else {
    block_header_t* new_block = allocate_block(aligned);
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
