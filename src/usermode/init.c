#include <stddef.h>
#include <stdint.h>

#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_DUP2    33
#define SYS_FORK    57
#define SYS_EXECVE  59
#define SYS_SLEEP   35
#define SYS_EXIT    60

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

static inline long syscall2(long number, long arg1, long arg2) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(number), "D"(arg1), "S"(arg2) : "rcx", "r11", "memory");
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

static void write_str(int fd, const char* text) {
  if(text == NULL) {
    return;
  }
  syscall3(SYS_WRITE, fd, (long)text, (long)str_len(text));
}

static void sleep_us(uint64_t usec) {
  syscall2(SYS_SLEEP, (long)usec, 0);
}

static void run_shell(void) {
  static char path[] = "/bin/user_demo";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  long rc = syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
  if(rc < 0) {
    write_str(STDERR_FILENO, "[init] execve(/bin/user_demo) failed\n");
    syscall1(SYS_EXIT, 1);
  }
}

void _start(void) {
  write_str(STDOUT_FILENO, "[init] meniOS init process online\n");

  long pid = syscall0(SYS_FORK);
  if(pid == 0) {
    run_shell();
  }

  if(pid < 0) {
    write_str(STDERR_FILENO, "[init] fork failed\n");
  }

  for(;;) {
    sleep_us(500000);
  }
}
