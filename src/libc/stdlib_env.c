#ifndef MENIOS_KERNEL

#include <kernel/syscall.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#define ENV_BUFFER_SIZE 256

static char getenv_buffer[ENV_BUFFER_SIZE];

char* getenv(const char* name) {
  if(name == NULL) {
    errno = EINVAL;
    return NULL;
  }

  register uint64_t rax asm("rax") = SYS_GETENV;
  register const char* rdi asm("rdi") = name;
  register char* rsi asm("rsi") = getenv_buffer;
  register size_t rdx asm("rdx") = sizeof(getenv_buffer);

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return NULL;
  }

  if(rax == 0) {
    errno = 0;
    return NULL;
  }

  errno = 0;
  return (char*)(uintptr_t)rax;
}

int setenv(const char* name, const char* value, int overwrite) {
  if(name == NULL || name[0] == '\0' || strchr(name, '=')) {
    errno = EINVAL;
    return -1;
  }

  if(value == NULL) {
    value = "";
  }

  register uint64_t rax asm("rax") = SYS_SETENV;
  register const char* rdi asm("rdi") = name;
  register const char* rsi asm("rsi") = value;
  register uint64_t rdx asm("rdx") = (uint64_t)overwrite;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return 0;
}

int unsetenv(const char* name) {
  if(name == NULL || name[0] == '\0' || strchr(name, '=')) {
    errno = EINVAL;
    return -1;
  }

  register uint64_t rax asm("rax") = SYS_UNSETENV;
  register const char* rdi asm("rdi") = name;

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

#endif
