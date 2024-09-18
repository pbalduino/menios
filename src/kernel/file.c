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


// static struct __fd_t fdstdout = {
//   .used = true,
//   .close = noop_close,
//   .read = noop_read,
//   .write = noop_write
// };

static struct __fd_t fdserial0 = {
  .used = true,
  .close = noop_close,
  .read = noop_read,
  .write = serial_putchar
};

static struct __sFile ffstdout = {
  .reserved = FD_STDOUT
};

static struct __sFile ffserial0 = {
  .reserved = -1
};


int noop_read() {
  serial_log("noop_read\n");
  return -1;
}

int noop_close() {
  serial_log("noop_close\n");
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
  serial_log("Entering file_init");

  ffstdout.reserved = FD_STDOUT;

  stdout = &ffstdout;
  descriptors[FD_STDOUT] = &fdserial0;

  printf("- Setting file descriptor...OK\n");
  serial_log("Leaving file_init");
}

file_descriptor_t fd_get(int fd) {
  if(fd >= 0 && fd < FD_LIMIT) {
    return descriptors[fd];
  }

  return NULL;
}

int dup2(int oldfd, int newfd) {
  serial_log("Entering dup2");
  if(oldfd >= 0 && oldfd < FD_LIMIT && newfd >= 0 && newfd < FD_LIMIT) {
    file_descriptor_t nd = descriptors[newfd];
    char str[256];
    itoa(oldfd, str, 10);
    itoa(newfd, str, 10);

    if(nd->close == NULL) {
      serial_error("Leaving dup2: file->close is null");
      return -1;
    }

    descriptors[newfd] = descriptors[oldfd];

      serial_log("Leaving dup2 with OK");
    return newfd;
  }

  serial_error("Leaving dup2: invalid parameter");
  return -1;
}

int get_free_fd() {
  serial_log("Entering get_free_fd");
  for(int p = 3; p < FD_LIMIT; p++) {
    if(descriptors[p] == NULL || !descriptors[p]->used) {
      serial_log("Leaving get_free_fd with OK");
      return p;
    }
  }
  serial_error("Leaving get_free_fd: no free descriptor");
  return -1;
}

// FIXME: file should not know about framebuffer. create a device list instead and use it.
FILE* fopen(const char* filename, const char* mode) {
  serial_log("Entering fopen");
  serial_log(filename);
  serial_log(mode);

  FILE* file = NULL;
  int fid = get_free_fd();

  if(fid < 0) {
    serial_error("Leaving fopen: no more available file descriptors");
    return file;
  }

  if(strcmp(filename, "/dev/fb") == 0 && strncmp(mode, "w", 1) == 0) {
    serial_log("Opening framebuffer");
    fd.used = true;
    fd.write = fb_putchar;
    fd.close = noop_close;
    fd.read = noop_read;

    descriptors[fid] = &fd;
    ff.reserved = fid;
    file = &ff;
  } else if(strcmp(filename, "/dev/ttyS0") == 0 && strncmp(mode, "w", 1) == 0) {
    serial_log("Opening serial");

    descriptors[fid] = &fdserial0;

    ffserial0.reserved = fid;

    file = &ffserial0;
  }

  serial_log("Leaving fopen");

  return file;
}

int fclose(FILE *stream) {
  return -1;
}

FILE* freopen(const char *filename, const char *mode, FILE *file) {
  serial_log("Entering freopen");
  FILE* new_file = fopen(filename, mode);

  if(new_file == NULL || file == NULL || dup2(new_file->reserved, file->reserved) < 0) {
    if(new_file == NULL) {
      serial_log("New file is null");
    }

    if(file == NULL) {
      serial_error("File is null");
    }

    serial_error("Leaving freopen");

    fclose(new_file);

    return NULL;
  }

  fclose(file);

  file = new_file;

  serial_log("Leaving freopen with OK");

  return new_file;
}
