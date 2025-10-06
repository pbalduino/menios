#include <stdint.h>
#include <stddef.h>

#define SYS_WRITE     1
#define SYS_EXIT      60
#define SYS_PROC_KILL 64

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

static inline long syscall1(long number, long arg1) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(number), "D"(arg1) : "rcx", "r11", "memory");
  return ret;
}

static inline long syscall2(long number, long arg1, long arg2) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret)
               : "a"(number), "D"(arg1), "S"(arg2)
               : "rcx", "r11", "memory");
  return ret;
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret)
               : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
               : "rcx", "r11", "memory");
  return ret;
}

static void write_str(int fd, const char* text) {
  if(text == NULL) {
    return;
  }
  const char* ptr = text;
  size_t len = 0;
  while(ptr[len] != '\0') {
    len++;
  }
  if(len == 0) {
    return;
  }
  syscall3(SYS_WRITE, fd, (long)text, (long)len);
}

static int parse_pid(const char* text) {
  if(text == NULL || text[0] == '\0') {
    return -1;
  }
  int value = 0;
  for(size_t i = 0; text[i] != '\0'; i++) {
    char ch = text[i];
    if(ch < '0' || ch > '9') {
      return -1;
    }
    value = value * 10 + (ch - '0');
  }
  return value;
}

void _start(uint64_t argc, char** argv, char** envp) {
  (void)envp;

  if(argc < 2 || argv[1] == NULL) {
    write_str(STDERR_FILENO, "usage: kill <pid>\n");
    syscall1(SYS_EXIT, 1);
  }

  int pid = parse_pid(argv[1]);
  if(pid < 0) {
    write_str(STDERR_FILENO, "kill: invalid pid\n");
    syscall1(SYS_EXIT, 1);
  }

  int code = 0;
  if(argc > 2 && argv[2] != NULL) {
    code = parse_pid(argv[2]);
    if(code < 0) {
      write_str(STDERR_FILENO, "kill: invalid exit code\n");
      syscall1(SYS_EXIT, 1);
    }
  }

  long rc = syscall2(SYS_PROC_KILL, pid, code);
  if(rc < 0) {
    write_str(STDERR_FILENO, "kill: operation failed\n");
    syscall1(SYS_EXIT, 1);
  }

  syscall1(SYS_EXIT, 0);
}
