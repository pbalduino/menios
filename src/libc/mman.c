#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/mman.h>

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
  return (void*)rax;
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
