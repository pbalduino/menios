#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

static void trim_trailing_slashes(char* path, size_t* length) {
  if(path == NULL || length == NULL) {
    return;
  }
  while(*length > 1 && path[*length - 1] == '/') {
    path[*length - 1] = '\0';
    (*length)--;
  }
}

static void pop_component(char* path, size_t* length) {
  if(path == NULL || length == NULL) {
    return;
  }
  if(*length <= 1) {
    path[0] = '/';
    path[1] = '\0';
    *length = 1;
    return;
  }

  size_t pos = *length;
  while(pos > 0 && path[pos - 1] != '/') {
    pos--;
  }

  if(pos <= 1) {
    path[0] = '/';
    path[1] = '\0';
    *length = 1;
    return;
  }

  path[pos - 1] = '\0';
  *length = pos - 1;
}

static bool append_component(char* path,
                             size_t* length,
                             const char* component,
                             size_t component_len,
                             size_t capacity) {
  if(path == NULL || length == NULL || component == NULL) {
    errno = EINVAL;
    return false;
  }

  if(component_len == 0) {
    return true;
  }

  if(*length >= capacity) {
    errno = ENAMETOOLONG;
    return false;
  }

  size_t needed = *length + component_len + 1;
  if(*length > 1) {
    needed += 1;
  }

  if(needed >= capacity) {
    errno = ENAMETOOLONG;
    return false;
  }

  if(*length > 1 && path[*length - 1] != '/') {
    path[(*length)++] = '/';
  }

  memcpy(path + *length, component, component_len);
  *length += component_len;
  path[*length] = '\0';
  return true;
}

char* realpath(const char* path, char* resolved_path) {
  if(path == NULL || *path == '\0') {
    errno = ENOENT;
    return NULL;
  }

  char* output = resolved_path;
  bool allocated = false;

  if(output == NULL) {
    output = (char*)malloc(PATH_MAX);
    if(output == NULL) {
      errno = ENOMEM;
      return NULL;
    }
    allocated = true;
  }

  char canonical[PATH_MAX];
  size_t canonical_len = 0;

  if(path[0] == '/') {
    canonical[0] = '/';
    canonical[1] = '\0';
    canonical_len = 1;
  } else {
    if(getcwd(canonical, sizeof(canonical)) == NULL) {
      if(allocated) {
        free(output);
      }
      return NULL;
    }
    canonical_len = strnlen(canonical, sizeof(canonical));
    if(canonical_len == 0) {
      canonical[0] = '/';
      canonical[1] = '\0';
      canonical_len = 1;
    } else {
      trim_trailing_slashes(canonical, &canonical_len);
    }
  }

  const char* iter = path;
  if(path[0] == '/') {
    while(*iter == '/') {
      iter++;
    }
  }

  char component[PATH_MAX];
  while(*iter != '\0') {
    size_t comp_len = 0;
    while(iter[comp_len] != '\0' && iter[comp_len] != '/') {
      if(comp_len + 1 >= sizeof(component)) {
        errno = ENAMETOOLONG;
        if(allocated) {
          free(output);
        }
        return NULL;
      }
      component[comp_len] = iter[comp_len];
      comp_len++;
    }
    component[comp_len] = '\0';

    if(comp_len == 0 || (comp_len == 1 && component[0] == '.')) {
      /* skip */
    } else if(comp_len == 2 && component[0] == '.' && component[1] == '.') {
      pop_component(canonical, &canonical_len);
    } else {
      if(!append_component(canonical, &canonical_len, component, comp_len, sizeof(canonical))) {
        if(allocated) {
          free(output);
        }
        return NULL;
      }
    }

    iter += comp_len;
    while(*iter == '/') {
      iter++;
    }
  }

  if(canonical_len == 0) {
    canonical[0] = '/';
    canonical[1] = '\0';
    canonical_len = 1;
  }

  size_t final_len = canonical_len;
  if(final_len + 1 > PATH_MAX) {
    errno = ENAMETOOLONG;
    if(allocated) {
      free(output);
    }
    return NULL;
  }

  memcpy(output, canonical, final_len + 1);

  struct stat st;
  if(stat(output, &st) < 0) {
    int err = errno;
    if(err != ENOENT) {
      if(allocated) {
        free(output);
      }
      errno = err;
      return NULL;
    }
    /* For ENOENT we still return the canonicalised path so callers that merely
       need a stable string (such as libiberty's lrealpath) can compare file
       names reliably.  Preserve the errno value so the caller can detect that
       the path does not currently exist. */
    errno = err;
  }

  return output;
}
