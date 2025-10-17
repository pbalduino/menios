#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static void fatal(const char* msg) {
  perror(msg);
  exit(1);
}

static void write_pattern(int fd, size_t bytes) {
  const size_t chunk = 4096;
  uint8_t buffer[chunk];
  size_t written = 0;
  size_t counter = 0;

  while(written < bytes) {
    size_t to_write = bytes - written;
    if(to_write > chunk) {
      to_write = chunk;
    }
    for(size_t i = 0; i < to_write; i++) {
      buffer[i] = (uint8_t)((counter + i) & 0xFF);
    }
    ssize_t rc = write(fd, buffer, to_write);
    if(rc < 0) {
      fatal("write");
    }
    if((size_t)rc != to_write) {
      fprintf(stderr, "write: short write %zd\n", rc);
      exit(1);
    }
    counter += to_write;
    written += to_write;
  }
}

static void verify_pattern(int fd, size_t bytes) {
  const size_t chunk = 4096;
  uint8_t buffer[chunk];
  size_t read_total = 0;
  size_t counter = 0;

  while(read_total < bytes) {
    size_t to_read = bytes - read_total;
    if(to_read > chunk) {
      to_read = chunk;
    }
    ssize_t rc = read(fd, buffer, to_read);
    if(rc < 0) {
      fatal("read");
    }
    if(rc == 0) {
      fprintf(stderr, "read: unexpected EOF after %zu bytes\n", read_total);
      exit(1);
    }
    for(ssize_t i = 0; i < rc; i++) {
      uint8_t expected = (uint8_t)((counter + (size_t)i) & 0xFF);
      if(buffer[i] != expected) {
        fprintf(stderr,
                "verify: mismatch at offset %zu (got %u expected %u)\n",
                read_total + (size_t)i,
                (unsigned)buffer[i],
                (unsigned)expected);
        exit(1);
      }
    }
    counter += (size_t)rc;
    read_total += (size_t)rc;
  }
}

int main(int argc, char** argv) {
  if(argc < 3) {
    fprintf(stderr, "Usage: %s <path> <size_bytes>\n", argv[0]);
    return 1;
  }

  const char* path = argv[1];
  size_t size = (size_t)strtoull(argv[2], NULL, 10);
  int fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644);
  if(fd < 0) {
    fatal("open");
  }

  printf("Writing %zu bytes to %s...\n", size, path);
  write_pattern(fd, size);

  printf("Syncing...\n");
  fsync(fd);

  printf("Verifying...\n");
  if(lseek(fd, 0, SEEK_SET) < 0) {
    fatal("lseek");
  }
  verify_pattern(fd, size);

  printf("Re-reading sequentially to trigger readahead...\n");
  if(lseek(fd, 0, SEEK_SET) < 0) {
    fatal("lseek");
  }
  verify_pattern(fd, size);

  printf("Done.\n");
  close(fd);
  return 0;
}

