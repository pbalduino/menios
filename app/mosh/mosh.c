#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef long ssize_t;
typedef int pid_t;

#define STDIN_FD   0
#define STDOUT_FD  1
#define STDERR_FD  2

#define SYS_READ     0
#define SYS_WRITE    1
#define SYS_OPEN     2
#define SYS_CLOSE    3
#define SYS_LSEEK    8
#define SYS_FORK    57
#define SYS_EXECVE  59
#define SYS_EXIT    60
#define SYS_KILL    62
#define SYS_DUP2    33
#define SYS_WAITPID 67
#define SYS_GETCWD  79
#define SYS_CHDIR   80
#define SYS_GETENV  81
#define SYS_SETENV  82
#define SYS_UNSETENV 83

#define O_RDONLY    0x0
#define O_WRONLY    0x1
#define O_RDWR      0x2

#define MOSH_MAX_LINE        512
#define MOSH_MAX_ARGS        16
#define MOSH_MAX_PATH_LEN    256
#define MOSH_MAX_PATH_PARTS  16
#define MOSH_EXEC_BUFFER     (512 * 1024)

static char line_buffer[MOSH_MAX_LINE];
static char exec_buffer[MOSH_EXEC_BUFFER];

extern const uint8_t user_demo_elf_start[];
extern const uint8_t user_demo_elf_end[];

static inline long syscall0(long number) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(number)
               : "rcx", "r11", "memory");
  return ret;
}

static inline long syscall1(long number, long arg1) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(number), "D"(arg1)
               : "rcx", "r11", "memory");
  return ret;
}

static inline long syscall2(long number, long arg1, long arg2) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(number), "D"(arg1), "S"(arg2)
               : "rcx", "r11", "memory");
  return ret;
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
               : "rcx", "r11", "memory");
  return ret;
}

static ssize_t sys_read(int fd, void* buffer, size_t length) {
  return (ssize_t)syscall3(SYS_READ, fd, (long)buffer, length);
}

static ssize_t sys_write(int fd, const void* buffer, size_t length) {
  return (ssize_t)syscall3(SYS_WRITE, fd, (long)buffer, length);
}

static int sys_open(const char* path, int flags, int mode) {
  return (int)syscall3(SYS_OPEN, (long)path, flags, mode);
}

static int sys_close(int fd) {
  return (int)syscall1(SYS_CLOSE, fd);
}

static int sys_dup2(int oldfd, int newfd) {
  return (int)syscall2(SYS_DUP2, oldfd, newfd);
}

static pid_t sys_fork(void) {
  return (pid_t)syscall0(SYS_FORK);
}

static int sys_execve(const void* image, size_t size) {
  return (int)syscall3(SYS_EXECVE, (long)image, (long)size, 0);
}

static void sys_exit(int code) {
  syscall1(SYS_EXIT, code);
  while(true) { }
}

static void attach_console(void) {
  static const char device[] = "/dev/console";
  int fd = sys_open(device, O_RDWR, 0);
  if(fd < 0) {
    return;
  }

  sys_dup2(fd, STDIN_FD);
  sys_dup2(fd, STDOUT_FD);
  sys_dup2(fd, STDERR_FD);

  if(fd > STDERR_FD) {
    sys_close(fd);
  }
}

static pid_t sys_waitpid(pid_t pid, int* status, int options) {
  return (pid_t)syscall3(SYS_WAITPID, pid, (long)status, options);
}

static int sys_getcwd(char* buffer, size_t size) {
  return (int)syscall2(SYS_GETCWD, (long)buffer, (long)size);
}

static int sys_chdir(const char* path) {
  return (int)syscall2(SYS_CHDIR, (long)path, 0);
}

static long sys_getenv(const char* name, char* buffer, size_t size) {
  return syscall3(SYS_GETENV, (long)name, (long)buffer, (long)size);
}

static int sys_setenv(const char* name, const char* value, int overwrite) {
  return (int)syscall3(SYS_SETENV, (long)name, (long)value, overwrite);
}

static int sys_unsetenv(const char* name) {
  return (int)syscall2(SYS_UNSETENV, (long)name, 0);
}

static size_t str_length(const char* s) {
  size_t len = 0;
  while(s[len]) {
    len++;
  }
  return len;
}

static int str_compare(const char* a, const char* b) {
  while(*a && (*a == *b)) {
    a++;
    b++;
  }
  return (int)((unsigned char)*a - (unsigned char)*b);
}

static bool str_equal(const char* a, const char* b) {
  return str_compare(a, b) == 0;
}

static void* mem_copy(void* dest, const void* src, size_t n) {
  uint8_t* d = (uint8_t*)dest;
  const uint8_t* s = (const uint8_t*)src;
  while(n--) {
    *d++ = *s++;
  }
  return dest;
}

static void* mem_set(void* dest, int value, size_t n) {
  uint8_t* d = (uint8_t*)dest;
  while(n--) {
    *d++ = (uint8_t)value;
  }
  return dest;
}

static bool is_whitespace(char ch) {
  return (ch == ' ' || ch == '\t' || ch == '\r');
}

static void write_str(int fd, const char* s) {
  sys_write(fd, s, str_length(s));
}

static ssize_t read_line(char* buffer, size_t size) {
  size_t pos = 0;
  while(pos + 1 < size) {
    char ch;
    ssize_t r = sys_read(STDIN_FD, &ch, 1);
    if(r <= 0) {
      return r;
    }

    if(ch == '\r') {
      continue;
    }

    if(ch == '\n') {
      buffer[pos] = '\0';
      sys_write(STDOUT_FD, "\n", 1);
      return (ssize_t)pos;
    }

    if(ch == 0x03) { /* Ctrl+C */
      sys_write(STDOUT_FD, "^C\n", 3);
      buffer[0] = '\0';
      return 0;
    }

    if(ch == 0x04) { /* Ctrl+D */
      return -1;
    }

    if(ch == '\b' || ch == 0x7f) {
      if(pos > 0) {
        pos--;
        sys_write(STDOUT_FD, "\b \b", 3);
      }
      continue;
    }

    if((unsigned char)ch < 32 || (unsigned char)ch >= 127) {
      continue;
    }

    buffer[pos++] = ch;
    sys_write(STDOUT_FD, &ch, 1);
  }

  buffer[pos] = '\0';
  return (ssize_t)pos;
}

static int tokenize(char* line, char* argv[], int max_args) {
  int argc = 0;
  char* cursor = line;

  while(*cursor != '\0') {
    while(is_whitespace(*cursor)) {
      cursor++;
    }

    if(*cursor == '\0') {
      break;
    }

    if(argc >= max_args) {
      write_str(STDERR_FD, "mosh: too many arguments\n");
      return -1;
    }

    argv[argc++] = cursor;

    while(*cursor && !is_whitespace(*cursor)) {
      cursor++;
    }

    if(*cursor == '\0') {
      break;
    }

    *cursor = '\0';
    cursor++;
  }

  argv[argc] = NULL;
  return argc;
}

static bool env_get_copy(const char* name, char* buffer, size_t size) {
  long rc = sys_getenv(name, buffer, size);
  if(rc <= 0) {
    return false;
  }
  return true;
}

static void ensure_default_environment(void) {
  char path[MOSH_MAX_PATH_LEN];
  if(!env_get_copy("PATH", path, sizeof(path))) {
    sys_setenv("PATH", "/bin:/", 0);
  }
}

static bool has_slash(const char* s) {
  while(*s) {
    if(*s == '/') {
      return true;
    }
    s++;
  }
  return false;
}

static bool copy_path(char* dest, size_t dest_size, const char* base, const char* name) {
  size_t base_len = str_length(base);
  size_t name_len = str_length(name);
  size_t need = base_len + name_len + 2;
  if(need > dest_size) {
    return false;
  }

  mem_copy(dest, base, base_len);
  size_t pos = base_len;
  if(pos == 0 || dest[pos - 1] != '/') {
    dest[pos++] = '/';
  }
  mem_copy(dest + pos, name, name_len);
  pos += name_len;
  dest[pos] = '\0';
  return true;
}

static bool resolve_from_path(const char* name, char* out, size_t out_size) {
  char path_copy[MOSH_MAX_PATH_LEN];
  if(!env_get_copy("PATH", path_copy, sizeof(path_copy))) {
    return false;
  }

  char* start = path_copy;
  while(true) {
    char* sep = start;
    while(*sep && *sep != ':') {
      sep++;
    }

    char saved = *sep;
    *sep = '\0';

    const char* dir = start;
    if(dir[0] == '\0') {
      dir = ".";
    }

    if(copy_path(out, out_size, dir, name)) {
      int fd = sys_open(out, O_RDONLY, 0);
      if(fd >= 0) {
        sys_close(fd);
        *sep = saved;
        return true;
      }
    }

    *sep = saved;
    if(saved == '\0') {
      break;
    }
    start = sep + 1;
  }

  return false;
}

static bool resolve_command(const char* name, char* out, size_t out_size, const uint8_t** builtin_image, size_t* builtin_size) {
  if(str_equal(name, "demo")) {
    if(builtin_image) {
      *builtin_image = user_demo_elf_start;
    }
    if(builtin_size) {
      *builtin_size = (size_t)(user_demo_elf_end - user_demo_elf_start);
    }
    return true;
  }

  if(has_slash(name)) {
    size_t len = str_length(name);
    if(len + 1 > out_size) {
      return false;
    }
    mem_copy(out, name, len + 1);
    if(builtin_image) {
      *builtin_image = NULL;
    }
    if(builtin_size) {
      *builtin_size = 0;
    }
    return true;
  }

  if(resolve_from_path(name, out, out_size)) {
    if(builtin_image) {
      *builtin_image = NULL;
    }
    if(builtin_size) {
      *builtin_size = 0;
    }
    return true;
  }

  return false;
}

static ssize_t load_program(const char* path, char* buffer, size_t buffer_size) {
  int fd = sys_open(path, O_RDONLY, 0);
  if(fd < 0) {
    return -1;
  }

  size_t total = 0;
  while(total < buffer_size) {
    ssize_t r = sys_read(fd, buffer + total, buffer_size - total);
    if(r < 0) {
      sys_close(fd);
      return -1;
    }
    if(r == 0) {
      break;
    }
    total += (size_t)r;
  }

  sys_close(fd);
  if(total == 0) {
    return -1;
  }
  return (ssize_t)total;
}

static void run_command(int argc, char* argv[]) {
  if(argc == 0) {
    return;
  }

  if(str_equal(argv[0], "exit")) {
    sys_exit(0);
  }

  if(str_equal(argv[0], "unset")) {
    if(argc > 1) {
      sys_unsetenv(argv[1]);
    }
    return;
  }

  char path[MOSH_MAX_PATH_LEN];
  const uint8_t* builtin_image = NULL;
  size_t builtin_size = 0;

  if(!resolve_command(argv[0], path, sizeof(path), &builtin_image, &builtin_size)) {
    write_str(STDERR_FD, "mosh: command not found: ");
    write_str(STDERR_FD, argv[0]);
    write_str(STDERR_FD, "\n");
    return;
  }

  ssize_t image_size = 0;
  if(builtin_image == NULL) {
    image_size = load_program(path, exec_buffer, sizeof(exec_buffer));
    if(image_size < 0) {
      write_str(STDERR_FD, "mosh: failed to load: ");
      write_str(STDERR_FD, path);
      write_str(STDERR_FD, "\n");
      return;
    }
  }

  pid_t pid = sys_fork();
  if(pid < 0) {
    write_str(STDERR_FD, "mosh: fork failed\n");
    return;
  }

  if(pid == 0) {
    const void* image = builtin_image ? (const void*)builtin_image : (const void*)exec_buffer;
    size_t size = builtin_image ? builtin_size : (size_t)image_size;
    int rc = sys_execve(image, size);
    if(rc < 0) {
      write_str(STDERR_FD, "mosh: exec failed\n");
    }
    sys_exit(127);
  }

  int status = 0;
  sys_waitpid(pid, &status, 0);
}

int main(void) {
  attach_console();
  ensure_default_environment();

  char cwd[MOSH_MAX_PATH_LEN];

  while(true) {
    if(sys_getcwd(cwd, sizeof(cwd)) < 0) {
      cwd[0] = '/';
      cwd[1] = '\0';
    }

    write_str(STDOUT_FD, cwd);
    write_str(STDOUT_FD, "$ ");

    ssize_t len = read_line(line_buffer, sizeof(line_buffer));
    if(len < 0) {
      write_str(STDOUT_FD, "exit\n");
      sys_exit(0);
    }

    if(len == 0) {
      continue;
    }

    char* argv[MOSH_MAX_ARGS + 1];
    int argc = tokenize(line_buffer, argv, MOSH_MAX_ARGS);
    if(argc <= 0) {
      continue;
    }

    run_command(argc, argv);
  }
}

void _start(void) {
  (void)main();
  sys_exit(0);
}
