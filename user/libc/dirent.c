#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <menios/syscall.h>
#include <menios/syscall_user.h>

struct DIR {
  char*  data;
  size_t length;
  size_t offset;
  struct dirent entry;
};

static int fetch_directory(const char* path, char** out_buffer, size_t* out_length) {
  long required = __menios_syscall3(SYS_LISTDIR, (long)path, 0, 0);
  if(required < 0) {
    errno = (int)(-required);
    return -1;
  }

  size_t buffer_size = (size_t)required;
  char* buffer = NULL;
  if(buffer_size > 0) {
    buffer = (char*)malloc(buffer_size + 1);
    if(buffer == NULL) {
      errno = ENOMEM;
      return -1;
    }

    long rc = __menios_syscall3(SYS_LISTDIR, (long)path, (long)buffer, (long)(buffer_size + 1));
    if(rc < 0) {
      int err = (int)(-rc);
      free(buffer);
      errno = err;
      return -1;
    }
    buffer_size = (size_t)rc;
    buffer[buffer_size] = '\0';
  }

  *out_buffer = buffer;
  *out_length = buffer_size;
  return 0;
}

DIR* opendir(const char* path) {
  if(path == NULL) {
    errno = EINVAL;
    return NULL;
  }

  DIR* dir = (DIR*)malloc(sizeof(DIR));
  if(dir == NULL) {
    errno = ENOMEM;
    return NULL;
  }
  memset(dir, 0, sizeof(DIR));

  if(fetch_directory(path, &dir->data, &dir->length) < 0) {
    free(dir);
    return NULL;
  }

  dir->offset = 0;
  memset(&dir->entry, 0, sizeof(dir->entry));
  return dir;
}

int closedir(DIR* dir) {
  if(dir == NULL) {
    errno = EINVAL;
    return -1;
  }
  free(dir->data);
  free(dir);
  return 0;
}

struct dirent* readdir(DIR* dir) {
  if(dir == NULL) {
    errno = EINVAL;
    return NULL;
  }

  if(dir->offset >= dir->length) {
    return NULL;
  }

  char* start = dir->data + dir->offset;
  char* end = memchr(start, '\n', dir->length - dir->offset);
  size_t len = 0;
  if(end != NULL) {
    len = (size_t)(end - start);
    dir->offset = (size_t)((end - dir->data) + 1);
  } else {
    len = dir->length - dir->offset;
    dir->offset = dir->length;
  }

  if(len >= sizeof(dir->entry.d_name)) {
    len = sizeof(dir->entry.d_name) - 1;
  }
  memcpy(dir->entry.d_name, start, len);
  dir->entry.d_name[len] = '\0';

  size_t name_len = strlen(dir->entry.d_name);
  dir->entry.d_type = DT_UNKNOWN;
  if(name_len > 0 && dir->entry.d_name[name_len - 1] == '/') {
    dir->entry.d_name[name_len - 1] = '\0';
    dir->entry.d_type = DT_DIR;
  }

  return &dir->entry;
}

void rewinddir(DIR* dir) {
  if(dir == NULL) {
    return;
  }
  dir->offset = 0;
}

long telldir(DIR* dir) {
  if(dir == NULL) {
    errno = EINVAL;
    return -1;
  }
  return (long)dir->offset;
}

void seekdir(DIR* dir, long loc) {
  if(dir == NULL || loc < 0) {
    return;
  }
  if((size_t)loc > dir->length) {
    dir->offset = dir->length;
  } else {
    dir->offset = (size_t)loc;
  }
}
