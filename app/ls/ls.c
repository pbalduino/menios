#include <stdint.h>
#include <stddef.h>

#define SYS_WRITE   1
#define SYS_LISTDIR 62
#define SYS_EXIT    60

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

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

static void str_copy(char* dest, size_t capacity, const char* src) {
  if(dest == NULL || capacity == 0) {
    return;
  }
  size_t idx = 0;
  if(src != NULL) {
    while(idx + 1 < capacity && src[idx] != '\0') {
      dest[idx] = src[idx];
      idx++;
    }
  }
  dest[idx] = '\0';
}

static void write_str(int fd, const char* text) {
  if(text == NULL) {
    return;
  }
  size_t len = str_len(text);
  syscall3(SYS_WRITE, fd, (long)text, (long)len);
}

static const char* get_env_value(char** envp, const char* key) {
  if(envp == NULL || key == NULL) {
    return NULL;
  }
  size_t key_len = 0;
  while(key[key_len] != '\0') {
    key_len++;
  }
  for(size_t i = 0; envp[i] != NULL; i++) {
    const char* entry = envp[i];
    size_t pos = 0;
    while(entry[pos] != '\0' && entry[pos] != '=') {
      pos++;
    }
    if(entry[pos] == '=' && pos == key_len) {
      bool match = true;
      for(size_t j = 0; j < key_len; j++) {
        if(entry[j] != key[j]) {
          match = false;
          break;
        }
      }
      if(match) {
        return entry + key_len + 1;
      }
    }
  }
  return NULL;
}

static bool normalize(const char* base, const char* path, char* out, size_t capacity) {
  if(path != NULL && path[0] == '/') {
    size_t idx = 0;
    while(path[idx] != '\0' && idx + 1 < capacity) {
      out[idx] = path[idx];
      idx++;
    }
    out[idx] = '\0';
    return path[idx] == '\0';
  }

  size_t len = 0;
  if(base == NULL || base[0] == '\0') {
    out[len++] = '/';
  } else {
    while(base[len] != '\0' && len + 1 < capacity) {
      out[len] = base[len];
      len++;
    }
    if(len == 0) {
      out[len++] = '/';
    }
  }
  if(len >= capacity) {
    out[capacity - 1] = '\0';
    return false;
  }
  out[len] = '\0';

  if(path == NULL || path[0] == '\0') {
    return true;
  }

  if(len > 1 && out[len - 1] != '/') {
    if(len + 1 >= capacity) {
      return false;
    }
    out[len++] = '/';
  }

  size_t i = 0;
  while(path[i] != '\0' && len + 1 < capacity) {
    out[len++] = path[i++];
  }
  out[len] = '\0';
  return path[i] == '\0';
}

void _start(uint64_t argc, char** argv, char** envp) {
  const char* cwd = get_env_value(envp, "PWD");
  if(cwd == NULL || cwd[0] == '\0') {
    cwd = "/";
  }

  char path_buffer[256];
  if(argc > 1 && argv[1] != NULL) {
    if(!normalize(cwd, argv[1], path_buffer, sizeof(path_buffer))) {
      write_str(STDERR_FILENO, "ls: path too long\n");
      syscall1(SYS_EXIT, 1);
    }
  } else {
    if(!normalize("/", cwd, path_buffer, sizeof(path_buffer))) {
      str_copy(path_buffer, sizeof(path_buffer), "/");
    }
  }

  static char buffer[4096];
  long rc = syscall3(SYS_LISTDIR, (long)path_buffer, (long)buffer, (long)sizeof(buffer) - 1);
  if(rc < 0) {
    write_str(STDERR_FILENO, "ls: unable to list directory\n");
    syscall1(SYS_EXIT, 1);
  }

  buffer[rc] = '\0';
  write_str(STDOUT_FILENO, buffer);
  syscall1(SYS_EXIT, 0);
}
