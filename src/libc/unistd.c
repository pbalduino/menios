#ifndef MENIOS_KERNEL
#include <kernel/syscall.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <unistd.h>

ssize_t read(int fd, void* buffer, size_t length) {
  register uint64_t rax asm("rax") = SYS_READ;
  register uint64_t rdi asm("rdi") = (uint64_t)fd;
  register void* rsi asm("rsi") = buffer;
  register size_t rdx asm("rdx") = length;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return (ssize_t)rax;
}

ssize_t write(int fd, const void* buffer, size_t length) {
  register uint64_t rax asm("rax") = SYS_WRITE;
  register uint64_t rdi asm("rdi") = (uint64_t)fd;
  register const void* rsi asm("rsi") = buffer;
  register size_t rdx asm("rdx") = length;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return (ssize_t)rax;
}

int close(int fd) {
  register uint64_t rax asm("rax") = SYS_CLOSE;
  register uint64_t rdi asm("rdi") = (uint64_t)fd;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return 0;
}

int dup(int fd) {
  register uint64_t rax asm("rax") = SYS_DUP;
  register uint64_t rdi asm("rdi") = (uint64_t)fd;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return (int)rax;
}

int dup2(int oldfd, int newfd) {
  register uint64_t rax asm("rax") = SYS_DUP2;
  register uint64_t rdi asm("rdi") = (uint64_t)oldfd;
  register uint64_t rsi asm("rsi") = (uint64_t)newfd;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return (int)rax;
}

int pipe(int pipefd[2]) {
  register uint64_t rax asm("rax") = SYS_PIPE;
  register int* rdi asm("rdi") = pipefd;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
  register uint64_t rax asm("rax") = SYS_LSEEK;
  register uint64_t rdi asm("rdi") = (uint64_t)fd;
  register off_t rsi asm("rsi") = offset;
  register uint64_t rdx asm("rdx") = (uint64_t)whence;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return (off_t)-1;
  }

  errno = 0;
  return (off_t)rax;
}

#endif
