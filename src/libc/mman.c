#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <unistd.h>

static size_t log_append_str(char* buffer, size_t pos, size_t capacity, const char* text) {
  while(text != NULL && *text != '\0' && pos < capacity) {
    buffer[pos++] = *text++;
  }
  return pos;
}

static size_t log_append_hex(char* buffer, size_t pos, size_t capacity, uint64_t value) {
  static const char digits[] = "0123456789abcdef";
  char tmp[16];
  size_t idx = 0u;

  if(value == 0) {
    tmp[idx++] = '0';
  } else {
    while(value != 0 && idx < sizeof(tmp)) {
      tmp[idx++] = digits[value & 0xFu];
      value >>= 4u;
    }
  }

  while(idx > 0u && pos < capacity) {
    buffer[pos++] = tmp[--idx];
  }

  return pos;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  register uint64_t rax asm("rax") = SYS_MMAP;
  register void* rdi asm("rdi") = addr;
  register size_t rsi asm("rsi") = length;
  register int rdx asm("rdx") = prot;
  register int r10 asm("r10") = flags;
  register int r8 asm("r8") = fd;
  register off_t r9 asm("r9") = offset;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return MAP_FAILED;
  }

  errno = 0;

  uintptr_t result = (uintptr_t)rax;
  uintptr_t result_hi = result >> 32;

  {
    char buf[128];
    size_t pos = 0u;
    pos = log_append_str(buf, pos, sizeof(buf), "mmap: addr=0x");
    pos = log_append_hex(buf, pos, sizeof(buf), (uint64_t)(uintptr_t)addr);
    pos = log_append_str(buf, pos, sizeof(buf), " len=0x");
    pos = log_append_hex(buf, pos, sizeof(buf), (uint64_t)length);
    pos = log_append_str(buf, pos, sizeof(buf), " ret=0x");
    pos = log_append_hex(buf, pos, sizeof(buf), (uint64_t)result);
    pos = log_append_str(buf, pos, sizeof(buf), " hi=0x");
    pos = log_append_hex(buf, pos, sizeof(buf), (uint64_t)result_hi);
    if(pos < sizeof(buf)) {
      buf[pos++] = '\n';
    }
    (void)write(2, buf, pos);
  }

  rax = result;
  return (void*)result;
}

int munmap(void* addr, size_t length) {
  register uint64_t rax asm("rax") = SYS_MUNMAP;
  register void* rdi asm("rdi") = addr;
  register size_t rsi asm("rsi") = length;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return 0;
}
#endif
