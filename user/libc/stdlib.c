#include <limits.h>
#include <menios/syscall.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

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

static inline bool is_power_of_two(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

static inline size_t align_up(size_t value, size_t alignment) {
  return (value + (alignment - 1)) & ~(alignment - 1);
}

typedef struct menios_block_header {
  void*  mapping_base;
  size_t mapping_size;
  size_t payload_size;
} menios_block_header_t;

static inline size_t default_page_size(void) {
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

static menios_block_header_t* map_block(size_t payload_size, size_t alignment) {
  if(!is_power_of_two(alignment)) {
    alignment = sizeof(void*);
  }

  if(payload_size == 0) {
    payload_size = alignment;
  }

  if(payload_size > SIZE_MAX - (alignment - 1)) {
    errno = ENOMEM;
    return NULL;
  }

  size_t padded = align_up(payload_size, alignment);
  size_t extra = alignment + sizeof(menios_block_header_t);
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

  uintptr_t base = (uintptr_t)mapping + sizeof(menios_block_header_t);
  uintptr_t aligned = align_up(base, alignment);
  menios_block_header_t* header = (menios_block_header_t*)(aligned - sizeof(menios_block_header_t));
  header->mapping_base = mapping;
  header->mapping_size = total;
  header->payload_size = padded;
  return header;
}

void* malloc(size_t size) {
  if(size == 0) {
    return NULL;
  }

  menios_block_header_t* header = map_block(size, 16);
  if(header == NULL) {
    return NULL;
  }

  return (void*)(header + 1);
}

void free(void* ptr) {
  if(ptr == NULL) {
    return;
  }

  menios_block_header_t* header = ((menios_block_header_t*)ptr) - 1;
  if(header->mapping_base != NULL && header->mapping_size != 0) {
    (void)munmap(header->mapping_base, header->mapping_size);
  }
}

void* aligned_alloc(size_t alignment, size_t size) {
  if(alignment < sizeof(void*) || !is_power_of_two(alignment) || size == 0 || (size % alignment) != 0) {
    errno = EINVAL;
    return NULL;
  }

  menios_block_header_t* header = map_block(size, alignment);
  if(header == NULL) {
    return NULL;
  }

  return (void*)(header + 1);
}

int posix_memalign(void** memptr, size_t alignment, size_t size) {
  if(memptr == NULL) {
    return EINVAL;
  }

  if(alignment < sizeof(void*) || !is_power_of_two(alignment)) {
    *memptr = NULL;
    return EINVAL;
  }

  menios_block_header_t* header = map_block(size, alignment);
  if(header == NULL) {
    *memptr = NULL;
    return errno != 0 ? errno : ENOMEM;
  }

  *memptr = (void*)(header + 1);
  return 0;
}

void* memalign(size_t alignment, size_t size) {
  if(alignment < sizeof(void*) || !is_power_of_two(alignment)) {
    errno = EINVAL;
    return NULL;
  }

  menios_block_header_t* header = map_block(size, alignment);
  if(header == NULL) {
    return NULL;
  }

  return (void*)(header + 1);
}

void* valloc(size_t size) {
  size_t page = default_page_size();
  menios_block_header_t* header = map_block(size == 0 ? page : size, page);
  if(header == NULL) {
    return NULL;
  }
  return (void*)(header + 1);
}

void* pvalloc(size_t size) {
  size_t page = default_page_size();
  if(size > SIZE_MAX - (page - 1)) {
    errno = ENOMEM;
    return NULL;
  }

  size_t rounded = size == 0 ? page : align_up(size, page);
  menios_block_header_t* header = map_block(rounded, page);
  if(header == NULL) {
    return NULL;
  }
  return (void*)(header + 1);
}

void* calloc(size_t nmemb, size_t size) {
  if(nmemb == 0 || size == 0) {
    return malloc(0);
  }

  if(size > 0 && nmemb > SIZE_MAX / size) {
    errno = ENOMEM;
    return NULL;
  }

  size_t total = nmemb * size;
  void* ptr = malloc(total);
  if(ptr) {
    memset(ptr, 0, total);
  }
  return ptr;
}

static size_t block_payload_size(const menios_block_header_t* header) {
  return header ? header->payload_size : 0;
}

void* realloc(void* ptr, size_t size) {
  if(ptr == NULL) {
    return malloc(size);
  }

  if(size == 0) {
    free(ptr);
    return NULL;
  }

  menios_block_header_t* header = ((menios_block_header_t*)ptr) - 1;
  size_t available = block_payload_size(header);
  if(size <= available) {
    return ptr;
  }

  void* replacement = malloc(size);
  if(replacement == NULL) {
    return NULL;
  }

  size_t copy = available < size ? available : size;
  memcpy(replacement, ptr, copy);
  free(ptr);
  return replacement;
}

void* reallocarray(void* ptr, size_t nmemb, size_t size) {
  if(nmemb == 0 || size == 0) {
    free(ptr);
    return NULL;
  }

  if(size > 0 && nmemb > SIZE_MAX / size) {
    errno = ENOMEM;
    return NULL;
  }

  return realloc(ptr, nmemb * size);
}

size_t malloc_usable_size(void* ptr) {
  if(ptr == NULL) {
    return 0;
  }

  menios_block_header_t* header = ((menios_block_header_t*)ptr) - 1;
  return block_payload_size(header);
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
