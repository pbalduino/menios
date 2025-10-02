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
#define SYS_WAITPID 61

#define STDIN_FILENO   0
#define STDOUT_FILENO  1
#define STDERR_FILENO  2

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002

static char env_path[] = "PATH=/bin";
static char env_home[] = "HOME=/";
static char* shell_envp[] = { env_path, env_home, NULL };

static char tty_path_console[] = "/dev/tty0";
static char tty_path_serial[] = "/dev/ttyS0";

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

static void write_log(const char* text) {
  write_str(STDERR_FILENO, text);
}

static void write_log_num(const char* prefix, long value) {
  char buffer[64];
  size_t idx = 0;
  while(prefix[idx] != '\0' && idx < sizeof(buffer) - 2) {
    buffer[idx] = prefix[idx];
    idx++;
  }
  if(value == 0) {
    buffer[idx++] = '0';
  } else {
    long temp = value;
    if(temp < 0) {
      buffer[idx++] = '-';
      temp = -temp;
    }
    char digits[32];
    size_t len = 0;
    while(temp > 0 && len < sizeof(digits)) {
      digits[len++] = (char)('0' + (temp % 10));
      temp /= 10;
    }
    while(len > 0 && idx < sizeof(buffer) - 2) {
      buffer[idx++] = digits[--len];
    }
  }
  buffer[idx++] = '\n';
  buffer[idx] = '\0';
  write_log(buffer);
}

static void sleep_us(uint64_t usec) {
  syscall2(SYS_SLEEP, (long)usec, 0);
}

static void bind_stdio(void) {
  long fd = syscall3(SYS_OPEN, (long)tty_path_console, O_RDWR, 0);
  if(fd < 0) {
    write_str(STDERR_FILENO, "[init] failed to open /dev/tty0\n");
    write_log_num("[init] tty0 open rc=", fd);
    return;
  }
  write_log("[init] opened /dev/tty0\n");

  for(long target = STDIN_FILENO; target <= STDERR_FILENO; target++) {
    if(fd == target) {
      continue;
    }
    long rc = syscall2(SYS_DUP2, fd, target);
    if(rc < 0) {
      write_str(STDERR_FILENO, "[init] dup2 failed\n");
      write_log_num("[init] dup2 failed target=", target);
    } else {
      write_log_num("[init] dup2 ok target=", target);
    }
  }

  if(fd > STDERR_FILENO) {
    syscall1(SYS_CLOSE, fd);
  }
  write_log("[init] tty0 dup complete\n");

  long serial_fd = syscall3(SYS_OPEN, (long)tty_path_serial, O_WRONLY, 0);
  if(serial_fd >= 0) {
    write_log("[init] opened /dev/ttyS0\n");
    long rc = syscall2(SYS_DUP2, serial_fd, STDERR_FILENO);
    if(rc < 0) {
      write_str(STDERR_FILENO, "[init] dup2 serial failed\n");
      write_log_num("[init] dup2 serial rc=", rc);
    }
    write_str(STDERR_FILENO, "[init] stderr -> /dev/ttyS0\n");
    write_log("[init] stderr duplicated to serial\n");
    if(serial_fd > STDERR_FILENO) {
      syscall1(SYS_CLOSE, serial_fd);
    }
  } else {
    write_str(STDERR_FILENO, "[init] failed to open /dev/ttyS0\n");
    write_log_num("[init] serial open rc=", serial_fd);
  }
  write_log("[init] bind_stdio finished\n");
}

static long exec_program(char* path) {
  char* argv[] = { path, NULL };
  write_log("[init] exec_program called\n");
  write_log(path);
  write_log("\n");
  return syscall3(SYS_EXECVE, (long)path, (long)argv, (long)shell_envp);
}

static void run_shell(void) {
  static char mosh_path[] = "/bin/mosh";
  static char fallback_path[] = "/bin/user_demo";

  write_log("[init] entering run_shell\n");
  write_str(STDERR_FILENO, "[init] entering run_shell\n");
  write_str(STDOUT_FILENO, "[init] launching mosh\n");
  write_log("[init] launching mosh\n");
  long rc = exec_program(mosh_path);
  write_log_num("[init] exec /bin/mosh rc=", rc);
  if(rc < 0) {
    write_str(STDERR_FILENO, "[init] execve(/bin/mosh) failed, falling back to user_demo\n");
    write_log("[init] mosh exec failed\n");
    rc = exec_program(fallback_path);
    write_log_num("[init] exec /bin/user_demo rc=", rc);
    if(rc < 0) {
      write_str(STDERR_FILENO, "[init] execve(/bin/user_demo) failed\n");
      write_log("[init] fallback exec failed\n");
      syscall1(SYS_EXIT, 1);
    }
  }
  write_log("[init] run_shell completed\n");
}

void _start(void) {
  bind_stdio();
  write_str(STDOUT_FILENO, "[init] meniOS init process online\n");
  write_log("[init] _start entering loop\n");

#ifdef TEMP_DISABLE_SUPERVISION
  write_log("[init] TEMP_DISABLE_SUPERVISION active\n");
  run_shell();
  write_log("[init] shell exited (TEMP_DISABLE_SUPERVISION)\n");
  syscall1(SYS_EXIT, 0);
#else
  for(;;) {
    write_log("[init] calling fork\n");
    long pid = syscall0(SYS_FORK);
    write_log_num("[init] fork rc=", pid);
    if(pid == 0) {
      write_str(STDERR_FILENO, "[init] child fork branch\n");
      write_log("[init] child fork branch\n");
      run_shell();
      write_str(STDERR_FILENO, "[init] child after run_shell\n");
      write_log("[init] child after run_shell\n");
      syscall1(SYS_EXIT, 1);
    }

    if(pid < 0) {
      write_str(STDERR_FILENO, "[init] fork failed\n");
      write_log("[init] fork failed path\n");
      sleep_us(500000);
      continue;
    }

    write_str(STDERR_FILENO, "[init] parent fork branch\n");
    write_log("[init] parent fork branch\n");

    int status = 0;
    long rc = syscall3(SYS_WAITPID, pid, (long)&status, 0);
    write_log_num("[init] waitpid rc=", rc);
    write_log_num("[init] waitpid status=", status);
    if(rc < 0) {
      write_str(STDERR_FILENO, "[init] waitpid failed\n");
      write_log("[init] waitpid failure loop\n");
      sleep_us(500000);
      continue;
    }

    write_str(STDOUT_FILENO, "[init] shell exited, restarting\n");
    write_log("[init] restarting shell\n");
  }
#endif
}
