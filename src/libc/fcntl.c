#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

int open(const char* path, int oflag, ...) {
  int mode = 0;
  if(oflag & O_CREAT) {
    va_list ap;
    va_start(ap, oflag);
    mode = va_arg(ap, int);
    va_end(ap);
  }

  register uint64_t rax asm("rax") = SYS_OPEN;
  register const char* rdi asm("rdi") = path;
  register int rsi asm("rsi") = oflag;
  register int rdx asm("rdx") = mode;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return (int)rax;
}

int fcntl(int fd, int cmd, ...) {
  uint64_t arg = 0;
  if(cmd == F_SETFD) {
    va_list ap;
    va_start(ap, cmd);
    arg = (uint64_t)va_arg(ap, int);
    va_end(ap);
  }

  register uint64_t rax asm("rax") = SYS_FCNTL;
  register uint64_t rdi asm("rdi") = (uint64_t)fd;
  register uint64_t rsi asm("rsi") = (uint64_t)cmd;
  register uint64_t rdx asm("rdx") = arg;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return (int)rax;
}

#endif
