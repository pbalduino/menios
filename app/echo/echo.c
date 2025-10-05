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

static void write_bytes(const char* data, size_t length) {
  if(data == NULL || length == 0) {
    return;
  }
  syscall3(SYS_WRITE, STDOUT_FILENO, (long)data, (long)length);
}

void _start(uint64_t argc, char** argv, char** envp) {
  (void)envp;

  if(argc <= 1) {
    write_bytes("\n", 1);
    syscall1(SYS_EXIT, 0);
  }

  for(uint64_t i = 1; i < argc; i++) {
    if(i > 1) {
      write_bytes(" ", 1);
    }
    const char* text = argv[i];
    if(text != NULL) {
      write_bytes(text, str_len(text));
    }
  }
  write_bytes("\n", 1);
  syscall1(SYS_EXIT, 0);
}
