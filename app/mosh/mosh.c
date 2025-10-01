#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SYS_READ   0
#define SYS_WRITE  1
#define SYS_CLOSE  3
#define SYS_DUP2   33
#define SYS_SLEEP  35
#define SYS_EXIT   60

#define STDIN_FILENO   0
#define STDOUT_FILENO  1
#define STDERR_FILENO  2

static inline long syscall0(long number) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(number) : "rcx", "r11", "memory");
  return ret;
}

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

static bool str_eq(const char* a, const char* b) {
  while(*a && *b) {
    if(*a++ != *b++) {
      return false;
    }
  }
  return *a == '\0' && *b == '\0';
}

static void write_str(int fd, const char* text) {
  if(text == NULL) {
    return;
  }
  syscall3(SYS_WRITE, fd, (long)text, (long)str_len(text));
}

static void trim_trailing_whitespace(char* buffer) {
  size_t len = str_len(buffer);
  while(len > 0) {
    char ch = buffer[len - 1];
    if(ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ') {
      buffer[len - 1] = '\0';
      len--;
      continue;
    }
    break;
  }
}

static void read_line(char* buffer, size_t capacity) {
  if(buffer == NULL || capacity == 0) {
    return;
  }

  long read_bytes = syscall3(SYS_READ, STDIN_FILENO, (long)buffer, (long)(capacity - 1));
  if(read_bytes <= 0) {
    buffer[0] = '\0';
    return;
  }
  buffer[read_bytes] = '\0';
  trim_trailing_whitespace(buffer);
}

static bool handle_builtin(const char* line) {
  if(str_eq(line, "")) {
    return true;
  }

  if(str_eq(line, "help")) {
    write_str(STDOUT_FILENO,
              "Built-ins:\n"
              "  help  - show this message\n"
              "  exit  - leave mosh\n");
    return true;
  }

  if(str_eq(line, "exit")) {
    write_str(STDOUT_FILENO, "bye\n");
    syscall1(SYS_EXIT, 0);
    return true; // Not reached
  }

  return false;
}

static void shell_loop(void) {
  static char line_buffer[256];

  write_str(STDOUT_FILENO, "This is mosh, the meniOS shell\nType 'help' for instructions.\n\n");

  while(true) {
    write_str(STDOUT_FILENO, "mosh> ");
    read_line(line_buffer, sizeof(line_buffer));

    if(line_buffer[0] == '\0') {
      continue;
    }

    if(handle_builtin(line_buffer)) {
      continue;
    }

    write_str(STDOUT_FILENO, "mosh: command lookup not implemented yet\n");
  }
}

void _start(void) {
  shell_loop();
  syscall1(SYS_EXIT, 0);
}
