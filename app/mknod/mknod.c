#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

static void usage(void) {
  fprintf(stderr, "usage: mknod <path> <major> <minor> [mode]\n");
}

static int parse_number(const char* text, long* value) {
  if(text == NULL || *text == '\0') {
    return -EINVAL;
  }

  char* end = NULL;
  errno = 0;
  long result = strtol(text, &end, 0);
  if(errno != 0 || end == text || *end != '\0') {
    return -EINVAL;
  }
  *value = result;
  return 0;
}

int main(int argc, char** argv) {
  if(argc < 4) {
    usage();
    return 1;
  }

  const char* path = argv[1];
  if(path == NULL || *path == '\0') {
    fprintf(stderr, "mknod: invalid path\n");
    return 1;
  }

  long major_value = 0;
  long minor_value = 0;
  if(parse_number(argv[2], &major_value) != 0 || parse_number(argv[3], &minor_value) != 0) {
    fprintf(stderr, "mknod: invalid major/minor numbers\n");
    return 1;
  }

  if(major_value < 0 || major_value > 0xfffff || minor_value < 0 || minor_value > 0xfffff) {
    fprintf(stderr, "mknod: major/minor out of range\n");
    return 1;
  }

  mode_t mode = S_IFCHR | 0666;
  if(argc >= 5) {
    long mode_value = 0;
    if(parse_number(argv[4], &mode_value) != 0) {
      fprintf(stderr, "mknod: invalid mode\n");
      return 1;
    }
    mode = S_IFCHR | (mode_t)(mode_value & 0777);
  }

  dev_t dev = MKDEV((unsigned int)major_value, (unsigned int)minor_value);
  if(mknod(path, mode, dev) != 0) {
    int err = errno;
    fprintf(stderr, "mknod: cannot create %s: ", path);
    errno = err;
    perror(NULL);
    return 1;
  }

  return 0;
}
