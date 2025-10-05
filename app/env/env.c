#include <stdint.h>
#include <stddef.h>

#define SYS_WRITE 1
#define SYS_EXIT  60

#define STDOUT_FILENO 1

static inline long syscall1(long number, long arg1) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(number), "D"(arg1) : "rcx", "r11", "memory");
  return ret;
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret)
               : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
               : "rcx", "r11", "memory");
  return ret;
}

static size_t str_len(const char* s) {
  size_t len = 0;
  while(s[len] != '\0') {
    len++;
  }
  return len;
}

static void write_line(const char* text) {
  if(text == NULL) {
    return;
  }
  syscall3(SYS_WRITE, STDOUT_FILENO, (long)text, (long)str_len(text));
  syscall3(SYS_WRITE, STDOUT_FILENO, (long)"\n", 1);
}

void _start(uint64_t argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;

  if(envp == NULL) {
    syscall1(SYS_EXIT, 0);
  }

  for(size_t i = 0; envp[i] != NULL; i++) {
    write_line(envp[i]);
  }

  syscall1(SYS_EXIT, 0);
}
