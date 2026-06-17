#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <unistd.h>

static const size_t BRK_INITIAL_SIZE = 16u * 1024u * 1024u; /* 16 MiB */
static const size_t BRK_GROW_SIZE = 4u * 1024u * 1024u;     /* 4 MiB per grow */
static const size_t BRK_MAX_SIZE = 128u * 1024u * 1024u;    /* 128 MiB cap */

typedef struct {
  void* base;
  void* current;
  void* limit;
  size_t mapped;
  bool initialized;
} brk_arena_state_t;

static brk_arena_state_t brk_state = {
  .base = NULL,
  .current = NULL,
  .limit = NULL,
  .mapped = 0u,
  .initialized = false,
};

static atomic_flag brk_lock_flag = ATOMIC_FLAG_INIT;

static void brk_lock(void) {
  while(atomic_flag_test_and_set_explicit(&brk_lock_flag, memory_order_acquire)) {
#ifndef MENIOS_HOST_TEST
    __asm__ __volatile__("pause");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
  }
}

static void brk_unlock(void) {
  atomic_flag_clear_explicit(&brk_lock_flag, memory_order_release);
}

static size_t brk_max_size(void) {
  return BRK_MAX_SIZE;
}

static size_t brk_page_size(void) {
  static size_t cached = 0u;
  if(cached != 0u) {
    return cached;
  }
#ifdef MENIOS_HOST_TEST
  cached = 4096u;
#else
  long rc = __menios_syscall0(SYS_GETPAGESIZE);
  if(rc > 0) {
    cached = (size_t)rc;
  } else {
    cached = 4096u;
  }
#endif
  return cached;
}

static bool brk_align_up(uintptr_t value, size_t alignment, uintptr_t* out) {
  if(out == NULL || alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
    return false;
  }
  uintptr_t mask = (uintptr_t)alignment - 1u;
  if(value > UINTPTR_MAX - mask) {
    return false;
  }
  *out = (value + mask) & ~mask;
  return true;
}

static size_t brk_align_size(size_t size) {
  size_t page = brk_page_size();
  if(page == 0u) {
    return 0u;
  }
  uintptr_t aligned = 0u;
  if(!brk_align_up((uintptr_t)size, page, &aligned)) {
    return 0u;
  }
  return (size_t)aligned;
}

static int brk_init_locked(void) {
  if(brk_state.initialized) {
    return 0;
  }

  size_t max_size = brk_max_size();
  size_t initial = brk_align_size(BRK_INITIAL_SIZE);
  if(initial == 0u || initial > max_size) {
    initial = brk_align_size(max_size);
  }

  if(initial == 0u) {
    errno = ENOMEM;
    return -1;
  }

  void* region = mmap(NULL,
                      initial,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,
                      0);
  if(region == MAP_FAILED) {
    return -1;
  }

  brk_state.base = region;
  brk_state.current = region;
  brk_state.limit = (char*)region + initial;
  brk_state.mapped = initial;
  brk_state.initialized = true;

  return 0;
}

static int brk_extend_locked(char* target) {
  size_t max_size = brk_max_size();
  if(brk_state.mapped >= max_size) {
    errno = ENOMEM;
    return -1;
  }

  while(target > (char*)brk_state.limit) {
    size_t remaining = max_size - brk_state.mapped;
    if(remaining == 0u) {
      errno = ENOMEM;
      return -1;
    }

    size_t grow = BRK_GROW_SIZE;
    if(grow > remaining) {
      grow = remaining;
    }
    grow = brk_align_size(grow);
    if(grow == 0u || grow > remaining) {
      errno = ENOMEM;
      return -1;
    }

    void* desired = (char*)brk_state.base + brk_state.mapped;
    void* region = mmap(desired,
                        grow,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    if(region == MAP_FAILED) {
      return -1;
    }

    if(region != desired) {
      (void)munmap(region, grow);
      errno = ENOMEM;
      return -1;
    }

    brk_state.mapped += grow;
    brk_state.limit = (char*)brk_state.base + brk_state.mapped;
  }

  return 0;
}

static int brk_set_locked(void* addr) {
  if(brk_init_locked() != 0) {
    return -1;
  }

  if(addr == NULL) {
    errno = EINVAL;
    return -1;
  }

  char* target = (char*)addr;
  char* base = (char*)brk_state.base;

  if(target < base) {
    errno = EINVAL;
    return -1;
  }

  uintptr_t offset = (uintptr_t)(target - base);
  if(offset > brk_max_size()) {
    errno = ENOMEM;
    return -1;
  }

  if(target > (char*)brk_state.limit) {
    if(brk_extend_locked(target) != 0) {
      return -1;
    }
  }

  brk_state.current = target;
  return 0;
}

int brk(void* addr) {
  brk_lock();
  int rc = brk_set_locked(addr);
  brk_unlock();
  return rc;
}

void* sbrk(intptr_t increment) {
  brk_lock();

  if(brk_init_locked() != 0) {
    brk_unlock();
    return (void*)-1;
  }

  char* current = (char*)brk_state.current;
  char* base = (char*)brk_state.base;
  void* result = current;

  if(increment == 0) {
    brk_unlock();
    return result;
  }

  char* target = NULL;
  if(increment > 0) {
    uintptr_t inc = (uintptr_t)increment;
    uintptr_t current_offset = (uintptr_t)(current - base);
    if(inc > brk_max_size() - current_offset) {
      errno = ENOMEM;
      brk_unlock();
      return (void*)-1;
    }
    target = current + inc;
  } else {
    uintptr_t dec = (uintptr_t)(-increment);
    uintptr_t current_offset = (uintptr_t)(current - base);
    if(dec > current_offset) {
      errno = EINVAL;
      brk_unlock();
      return (void*)-1;
    }
    target = current - dec;
  }

  if(brk_set_locked(target) != 0) {
    brk_unlock();
    return (void*)-1;
  }

  brk_unlock();
  return result;
}

#endif
