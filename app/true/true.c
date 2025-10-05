#include <stdint.h>

#define SYS_EXIT 60

static inline long syscall1(long number, long arg1) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(number), "D"(arg1) : "rcx", "r11", "memory");
  return ret;
}

void _start(uint64_t argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  (void)envp;
  syscall1(SYS_EXIT, 0);
}
