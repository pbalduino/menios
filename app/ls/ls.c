#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <menios/syscall.h>
#include <menios/syscall_user.h>

static const char* find_env_value(char** envp, const char* key) {
  if(envp == NULL || key == NULL) {
    return NULL;
  }
  size_t key_len = strlen(key);
  for(size_t i = 0; envp[i] != NULL; i++) {
    const char* entry = envp[i];
    if(strncmp(entry, key, key_len) == 0 && entry[key_len] == '=') {
      return entry + key_len + 1;
    }
  }
  return NULL;
}

static bool normalize_path(const char* base, const char* path, char* out, size_t capacity) {
  if(path != NULL && path[0] == '/') {
    size_t len = strlen(path);
    if(len + 1 > capacity) {
      return false;
    }
    memcpy(out, path, len + 1);
    return true;
  }

  const char* root = (base != NULL && base[0] != '\0') ? base : "/";
  size_t base_len = strlen(root);
  size_t path_len = (path != NULL) ? strlen(path) : 0;
  bool need_slash = (path_len > 0 && base_len > 0 && root[base_len - 1] != '/');

  size_t total = base_len + (need_slash ? 1 : 0) + path_len;
  if(total + 1 > capacity) {
    return false;
  }

  memcpy(out, root, base_len);
  size_t pos = base_len;
  if(need_slash) {
    out[pos++] = '/';
  }
  if(path_len > 0) {
    memcpy(out + pos, path, path_len);
    pos += path_len;
  }
  out[pos] = '\0';
  return true;
}

static int list_directory(const char* path) {
  static char buffer[4096];
  long rc = __menios_syscall3(SYS_LISTDIR, (long)path, (long)buffer, (long)sizeof(buffer) - 1);
  if(rc < 0) {
    fprintf(stderr, "ls: unable to list directory %s\n", path);
    return 1;
  }
  if(rc > 0) {
    if(write(STDOUT_FILENO, buffer, (size_t)rc) < 0) {
      perror("ls");
      return 1;
    }
  }
  return 0;
}

int main(int argc, char** argv, char** envp) {
  const char* cwd = find_env_value(envp, "PWD");
  if(cwd == NULL || cwd[0] == '\0') {
    cwd = "/";
  }

  char path_buffer[256];
  const char* target = (argc > 1 && argv[1] != NULL) ? argv[1] : cwd;

  if(!normalize_path(cwd, target, path_buffer, sizeof(path_buffer))) {
    fprintf(stderr, "ls: path too long\n");
    return 1;
  }

  return list_directory(path_buffer);
}
