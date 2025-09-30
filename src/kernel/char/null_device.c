#include <stddef.h>
#include <string.h>
#include <sys/errno.h>

#include <kernel/char_device.h>
#include <kernel/file.h>
#include <kernel/null_device.h>

static int64_t null_read(file_t* file, void* buffer, size_t length) {
  (void)file;
  (void)buffer;
  (void)length;
  return 0;
}

static int64_t null_write(file_t* file, const void* buffer, size_t length) {
  (void)file;
  if(buffer == NULL && length > 0) {
    return -EINVAL;
  }
  return (int64_t)length;
}

static const file_ops_t null_file_ops = {
  .read = null_read,
  .write = null_write,
  .close = NULL,
  .seek = NULL,
};

static int null_char_open_cb(char_device_t* device, uint32_t mode, file_t** out_file) {
  (void)device;
  file_t* file = file_create(&null_file_ops, NULL, mode ? mode : (FILE_MODE_READ | FILE_MODE_WRITE));
  if(file == NULL) {
    return -ENOMEM;
  }
  *out_file = file;
  return 0;
}

static const char_device_ops_t null_char_ops = {
  .open = null_char_open_cb,
};

static char_device_t null_char_instance = {
  .name = "/dev/null",
  .supported_modes = FILE_MODE_READ | FILE_MODE_WRITE,
  .ops = &null_char_ops,
  .driver_ctx = NULL,
  .next = NULL,
};

char_device_t* null_char_device(void) {
  return &null_char_instance;
}
