#include <stddef.h>
#include <string.h>
#include <sys/errno.h>

#include <kernel/char_device.h>
#include <kernel/file.h>
#include <kernel/zero_device.h>

static int64_t zero_read(file_t* file, void* buffer, size_t length) {
  (void)file;
  if(buffer == NULL) {
    return -EINVAL;
  }
  memset(buffer, 0, length);
  return (int64_t)length;
}

static int64_t zero_write(file_t* file, const void* buffer, size_t length) {
  (void)file;
  (void)buffer;
  return (int64_t)length;
}

static const file_ops_t zero_file_ops = {
  .read = zero_read,
  .write = zero_write,
  .close = NULL,
  .seek = NULL,
};

static int zero_char_open_cb(char_device_t* device, uint32_t mode, file_t** out_file) {
  (void)device;
  file_t* file = file_create(&zero_file_ops, NULL, mode ? mode : (FILE_MODE_READ | FILE_MODE_WRITE));
  if(file == NULL) {
    return -ENOMEM;
  }
  *out_file = file;
  return 0;
}

static const char_device_ops_t zero_char_ops = {
  .open = zero_char_open_cb,
};

static char_device_t zero_char_instance = {
  .name = "/dev/zero",
  .supported_modes = FILE_MODE_READ | FILE_MODE_WRITE,
  .ops = &zero_char_ops,
  .driver_ctx = NULL,
  .next = NULL,
};

char_device_t* zero_char_device(void) {
  return &zero_char_instance;
}
