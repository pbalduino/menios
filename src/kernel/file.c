#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <kernel/file.h>
#include <kernel/framebuffer.h>
#include <kernel/serial.h>

#define FD_STDIN       0
#define FD_STDOUT      1
#define FD_STDERR      2

#define FD_LIMIT 0xff

int noop_read();
int noop_close();
int noop_write(int ch);

FILE* stdin;
FILE* stdout;
FILE* stderr;

static file_descriptor_t descriptors[FD_LIMIT + 1];
static struct __fd_t fd;
static struct __sFile ff;
static struct __fd_t fdstdout = {
  .used = true,
  .close = noop_close,
  .read = noop_read,
  .write = noop_write
};

static struct __sFile ffstdout = {
  .reserved = FD_STDOUT
};


int noop_read() {
  serial_puts("noop_read\n");
  return -1;
}

int noop_close() {
  serial_puts("noop_close\n");
  return -1;
}

int noop_write(int ch) {
  serial_puts("noop_write\n");
  serial_puts("  ");
  serial_putchar(ch);
  serial_puts("\n");
  return -1;
}

void file_init() {
  serial_puts("file_init\n");

  ffstdout.reserved = FD_STDOUT;

  serial_puts("  52\n");
  stdout = &ffstdout;
  serial_puts("  55\n");
  descriptors[FD_STDOUT] = &fdstdout;
  serial_puts("  OK\n");

  printf("- Setting file descriptor...OK\n");
}

file_descriptor_t fd_get(int fd) {
  if(fd >= 0 && fd < FD_LIMIT) {
    return descriptors[fd];
  }

  return NULL;
}

int dup2(int oldfd, int newfd) {
  serial_puts("dup2\n");
  if(oldfd >= 0 && oldfd < FD_LIMIT && newfd >= 0 && newfd < FD_LIMIT) {
    file_descriptor_t nd = descriptors[newfd];
    serial_puts("<- dup2\n");

    char str[256];
    serial_puts("  86\n");
    itoa(oldfd, str, 10);
    serial_puts("  88\n");
    serial_puts(str);
    serial_puts("  90\n");
    itoa(newfd, str, 10);
    serial_puts("  92\n");
    serial_puts(str);
    serial_puts("  94\n");

    if(nd->close == NULL) {
      serial_puts("file->close is null in file.c:41");
      return -1;
    }

    serial_puts("  101");
    descriptors[newfd] = descriptors[oldfd];

    serial_puts("  103");
    return newfd;
  }
  serial_puts("invalid parameter in file.c:48");
  return -1;
}

int get_free_fd() {
  serial_puts("get_free_fd\n");
  for(int p = 3; p < FD_LIMIT; p++) {
    if(descriptors[p] == NULL || !descriptors[p]->used) {
      serial_puts("  OK\n");
      return p;
    }
  }
  serial_puts("  ERR\n");
  return -1;
}

// FIXME: file should not know about framebuffer. create a device list instead and use it.

FILE* fopen(const char* filename, const char* mode) {
  serial_puts("fopen\n");
  FILE* file = NULL;
  int fid = get_free_fd();

  serial_puts("fopen\n");
  if(fid < 0) {
    serial_puts("  ERR\n");
    return file;
  }

  serial_puts("  file: ");
  serial_puts(filename);
  serial_puts(" mode: ");
  serial_puts(mode);
  serial_puts("\n");

  if(strcmp(filename, "/dev/fb") == 0 && strcmp(mode, "w") == 0) {
    fd.used = true;
    fd.write = fb_putchar;
    fd.close = noop_close;
    fd.read = noop_read;

    serial_puts("  105\n");
    descriptors[fid] = &fd;
    serial_puts("  113\n");
    ff.reserved = fid;
    serial_puts("  115\n");
    file = &ff;
    serial_puts("  OK\n");
  }

  serial_puts("122\n");

  return file;
}
FILE* freopen(const char *filename, const char *mode, FILE *file) {
  serial_puts("freopen\n");
  FILE* new_file = fopen(filename, mode);
  serial_puts("freopen\n");

  if(new_file == NULL || file == NULL || dup2(new_file->reserved, file->reserved) < 0) {
    if(new_file == NULL) {
      serial_puts("  new_file is null\n");
    }

    if(file == NULL) {
      serial_puts("  file is null\n");
    }

    serial_puts("  ERR\n");
    return NULL;
  }

  serial_puts("  OK\n");
  return new_file;
}
