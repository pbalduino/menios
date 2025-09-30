#include <sys/errno.h>
#include <sys/fcntl.h>

#include <kernel/console.h>
#include <kernel/file.h>
#include <kernel/framebuffer.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/driver/ps2kb.h>
#include <kernel/tty.h>

static int64_t console_read(file_t* file, void* buffer, size_t length) {
  (void)file;
  if(buffer == NULL || length == 0) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  uint8_t* out = (uint8_t*)buffer;
  size_t count = 0;

  while(count < length) {
    int ch = kgetchar();
    out[count++] = (uint8_t)ch;
    if(ch == '\n') {
      break;
    }
  }

  return (int64_t)count;
}

static int64_t console_write(file_t* file, const void* buffer, size_t length) {
  (void)file;
  if(buffer == NULL) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  if(length == 0) {
    return 0;
  }

  const uint8_t* data = (const uint8_t*)buffer;
  tty_push_bytes(data, length);
  return (int64_t)length;
}

static const file_ops_t console_file_ops = {
  .read = console_read,
  .write = console_write,
  .close = NULL,
  .seek = NULL,
};

file_t* console_device_open(void) {
  file_t* file = file_create(&console_file_ops, NULL, FILE_MODE_READ | FILE_MODE_WRITE);
  if(file == NULL && current != NULL && current->errno == 0) {
    current->errno = ENOMEM;
  }
  return file;
}
