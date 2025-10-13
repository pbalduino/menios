#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int dump_meminfo(void) {
  int fd = open("/proc/meminfo", O_RDONLY);
  if(fd < 0) {
    perror("mem: failed to open /proc/meminfo");
    return 1;
  }

  char buffer[256];
  for(;;) {
    ssize_t read_bytes = read(fd, buffer, sizeof(buffer));
    if(read_bytes < 0) {
      if(errno == EINTR) {
        continue;
      }
      perror("mem: error reading /proc/meminfo");
      close(fd);
      return 1;
    }
    if(read_bytes == 0) {
      break;
    }

    ssize_t written = 0;
    while(written < read_bytes) {
      ssize_t result = write(STDOUT_FILENO, buffer + written, (size_t)(read_bytes - written));
      if(result < 0) {
        if(errno == EINTR) {
          continue;
        }
        perror("mem: failed to write output");
        close(fd);
        return 1;
      }
      written += result;
    }
  }

  close(fd);
  return 0;
}

int main(void) {
  return dump_meminfo();
}
