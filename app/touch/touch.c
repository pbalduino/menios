#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static int touch_path(const char* path) {
  if(path == NULL || *path == '\0') {
    errno = EINVAL;
    return -1;
  }

  int fd = open(path, O_WRONLY | O_CREAT, 0644);
  if(fd < 0) {
    return -1;
  }

  if(close(fd) < 0) {
    return -1;
  }

  return 0;
}

int main(int argc, char** argv) {
  if(argc <= 1) {
    fprintf(stderr, "touch: missing file operand\n");
    return 1;
  }

  int exit_code = 0;
  for(int i = 1; i < argc; i++) {
    const char* path = argv[i];
    if(path == NULL || *path == '\0') {
      fprintf(stderr, "touch: invalid path\n");
      exit_code = 1;
      continue;
    }

    if(touch_path(path) != 0) {
      int err = errno;
      fprintf(stderr, "touch: cannot create %s: ", path);
      errno = err;
      perror(NULL);
      exit_code = 1;
    }
  }

  return exit_code;
}
