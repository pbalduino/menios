#include <stdint.h>
#include <stddef.h>

#define SYS_READ   0
#define SYS_WRITE  1
#define SYS_OPEN   2
#define SYS_CLOSE  3
#define SYS_EXIT   60

#define STDIN_FILENO   0
#define STDOUT_FILENO  1
#define STDERR_FILENO  2

#define O_RDONLY 0x0000

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

static void write_bytes(int fd, const char* data, size_t length) {
  if(data == NULL || length == 0) {
    return;
  }
  syscall3(SYS_WRITE, fd, (long)data, (long)length);
}

static void write_cstring(int fd, const char* text) {
  write_bytes(fd, text, str_len(text));
}

static int copy_fd(int fd) {
  char buffer[512];
  while(1) {
    long rc = syscall3(SYS_READ, fd, (long)buffer, (long)sizeof(buffer));
    if(rc < 0) {
      return -1;
    }
    if(rc == 0) {
      break;
    }
    write_bytes(STDOUT_FILENO, buffer, (size_t)rc);
  }
  return 0;
}

void _start(uint64_t argc, char** argv, char** envp) {
  (void)envp;

  if(argc <= 1) {
    copy_fd(STDIN_FILENO);
    syscall1(SYS_EXIT, 0);
  }

  for(uint64_t i = 1; i < argc; i++) {
    const char* path = argv[i];
    if(path == NULL) {
      continue;
    }
    long fd = syscall3(SYS_OPEN, (long)path, O_RDONLY, 0);
    if(fd < 0) {
      write_cstring(STDERR_FILENO, "cat: unable to open ");
      write_cstring(STDERR_FILENO, path);
      write_cstring(STDERR_FILENO, "\n");
      continue;
    }
    copy_fd((int)fd);
    if(fd > STDERR_FILENO) {
      syscall1(SYS_CLOSE, fd);
    }
  }

  syscall1(SYS_EXIT, 0);
}
