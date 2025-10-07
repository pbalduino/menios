#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int copy_fd(int fd) {
  char buffer[512];
  while(1) {
    ssize_t rc = read(fd, buffer, sizeof(buffer));
    if(rc < 0) {
      return -1;
    }
    if(rc == 0) {
      break;
    }
    ssize_t written = write(STDOUT_FILENO, buffer, (size_t)rc);
    if(written < rc) {
      return -1;
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  if(argc <= 1) {
    return (copy_fd(STDIN_FILENO) < 0) ? 1 : 0;
  }

  int exit_code = 0;
  for(int i = 1; i < argc; i++) {
    const char* path = argv[i];
    if(path == NULL) {
      continue;
    }
    int fd = open(path, O_RDONLY);
    if(fd < 0) {
      fprintf(stderr, "cat: unable to open %s: ", path);
      perror(NULL);
      exit_code = 1;
      continue;
    }
    if(copy_fd(fd) < 0) {
      fprintf(stderr, "cat: read error on %s\n", path);
      exit_code = 1;
    }
    if(fd > STDERR_FILENO) {
      close(fd);
    }
  }

  return exit_code;
}
