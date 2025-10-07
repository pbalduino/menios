#include <limits.h>
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>

void __menios_fini_libc(int status);

static inline bool is_power_of_two(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

static inline size_t align_up(size_t value, size_t alignment) {
  return (value + (alignment - 1)) & ~(alignment - 1);
}

typedef struct menios_block_header {
  void*  mapping_base;
  size_t mapping_size;
} menios_block_header_t;

static menios_block_header_t* map_block(size_t payload_size, size_t alignment) {
  if(!is_power_of_two(alignment)) {
    alignment = sizeof(void*);
  }

  if(payload_size == 0) {
    payload_size = alignment;
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
