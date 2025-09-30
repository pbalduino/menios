#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>

#include <kernel/condvar.h>
#include <kernel/console.h>
#include <kernel/char_device.h>
#include <kernel/file.h>
#include <kernel/framebuffer.h>
#include <kernel/input/keyboard.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>
#include <kernel/tty.h>
#include <kernel/vga_text.h>
#include <kernel/null_device.h>
#include <kernel/zero_device.h>
#include <kernel/vfs.h>

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

static const file_ops_t serial_file_ops;
static const file_ops_t framebuffer_file_ops;

#ifdef MENIOS_KERNEL
static FILE kernel_stdin_stream = { .reserved = FD_STDIN };
static FILE kernel_stdout_stream = { .reserved = FD_STDOUT };
static FILE kernel_stderr_stream = { .reserved = FD_STDERR };

FILE* stdin = &kernel_stdin_stream;
FILE* stdout = &kernel_stdout_stream;
FILE* stderr = &kernel_stderr_stream;
#endif

static struct proc_info_t* owning_proc(void) {
  if(current != NULL) {
    return current;
  }
  return &kernel_process_info;
}

static inline void set_errno(int err) {
  if(current) {
    current->errno = err;
  }
}

file_t* file_create(const file_ops_t* ops, void* private_data, uint32_t mode) {
  file_t* handle = kmalloc(sizeof(file_t));
  if(handle == NULL) {
    set_errno(ENOMEM);
    return NULL;
  }

  handle->ops = ops;
  handle->private_data = private_data;
  handle->refcount = 1;
  handle->mode = mode;
  return handle;
}

void file_ref(file_t* file) {
  if(file == NULL) {
    return;
  }
  __atomic_add_fetch(&file->refcount, 1, __ATOMIC_SEQ_CST);
}

static void file_destroy(file_t* file) {
  if(file->ops && file->ops->close) {
    file->ops->close(file);
  }
  kfree(file);
}

void file_unref(file_t* file) {
  if(file == NULL) {
    return;
  }
  int64_t refs = __atomic_sub_fetch(&file->refcount, 1, __ATOMIC_SEQ_CST);
  if(refs == 0) {
    file_destroy(file);
  }
}

int64_t file_read(file_t* file, void* buffer, size_t length) {
  if(file == NULL || buffer == NULL || length == 0) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if((file->mode & FILE_MODE_READ) == 0 || file->ops == NULL || file->ops->read == NULL) {
    set_errno(EBADF);
    return -EBADF;
  }
  int64_t result = file->ops->read(file, buffer, length);
  if(result < 0) {
    set_errno((int)-result);
  } else if(current) {
    current->errno = 0;
  }
  return result;
}

int64_t file_write(file_t* file, const void* buffer, size_t length) {
  if(file == NULL || buffer == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if((file->mode & FILE_MODE_WRITE) == 0 || file->ops == NULL || file->ops->write == NULL) {
    set_errno(EBADF);
    return -EBADF;
  }
  int64_t result = file->ops->write(file, buffer, length);
  if(result < 0) {
    set_errno((int)-result);
  } else if(current) {
    current->errno = 0;
  }
  return result;
}

int64_t file_seek(file_t* file, int64_t offset, int whence) {
  if(file == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if(file->ops == NULL || file->ops->seek == NULL) {
    set_errno(ESPIPE);
    return -ESPIPE;
  }

  int64_t result = file->ops->seek(file, offset, whence);
  if(result < 0) {
    set_errno((int)-result);
  } else if(current) {
    current->errno = 0;
  }
  return result;
}

void proc_file_table_init(struct proc_info_t* proc) {
  if(proc == NULL) {
    return;
  }
  for(size_t fd = 0; fd < PROC_MAX_FILES; fd++) {
    proc->files[fd].file = NULL;
    proc->files[fd].flags = 0;
  }
}

void proc_file_table_clone(struct proc_info_t* child, struct proc_info_t* parent) {
  if(child == NULL || parent == NULL) {
    return;
  }
  for(size_t fd = 0; fd < PROC_MAX_FILES; fd++) {
    file_t* file = parent->files[fd].file;
    child->files[fd].file = file;
    child->files[fd].flags = parent->files[fd].flags;
    if(file != NULL) {
      file_ref(file);
    }
  }
}

void proc_file_table_cleanup(struct proc_info_t* proc) {
  if(proc == NULL) {
    return;
  }
  for(size_t fd = 0; fd < PROC_MAX_FILES; fd++) {
    if(proc->files[fd].file != NULL) {
      file_unref(proc->files[fd].file);
      proc->files[fd].file = NULL;
      proc->files[fd].flags = 0;
    }
  }
}

void proc_file_table_prepare_exec(struct proc_info_t* proc) {
  if(proc == NULL) {
    return;
  }
  for(size_t fd = 0; fd < PROC_MAX_FILES; fd++) {
    if(proc->files[fd].file != NULL && (proc->files[fd].flags & FD_FLAG_CLOEXEC)) {
      file_unref(proc->files[fd].file);
      proc->files[fd].file = NULL;
      proc->files[fd].flags = 0;
    }
  }
}

static int find_free_fd(struct proc_info_t* proc, int start) {
  for(int fd = start; fd < PROC_MAX_FILES; fd++) {
    if(proc->files[fd].file == NULL) {
      return fd;
    }
  }
  return -EMFILE;
}

int proc_file_install_at(struct proc_info_t* proc, int fd, file_t* file, uint32_t flags) {
  if(proc == NULL || file == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if(fd < 0 || fd >= PROC_MAX_FILES) {
    set_errno(EBADF);
    return -EBADF;
  }
  if(proc->files[fd].file != NULL) {
    set_errno(EBADF);
    return -EBADF;
  }
  file_ref(file);
  proc->files[fd].file = file;
  proc->files[fd].flags = flags;
  if(current) {
    current->errno = 0;
  }
  return fd;
}

int proc_file_install(struct proc_info_t* proc, file_t* file, uint32_t flags) {
  if(proc == NULL || file == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  int fd = find_free_fd(proc, 0);
  if(fd < 0) {
    set_errno(-fd);
    return fd;
  }
  return proc_file_install_at(proc, fd, file, flags);
}

file_t* proc_file_get(struct proc_info_t* proc, int fd, uint32_t* flags_out) {
  if(proc == NULL || fd < 0 || fd >= PROC_MAX_FILES) {
    set_errno(EBADF);
    return NULL;
  }
  file_t* file = proc->files[fd].file;
  if(file == NULL) {
    set_errno(EBADF);
    return NULL;
  }
  if(flags_out) {
    *flags_out = proc->files[fd].flags;
  }
  file_ref(file);
  if(current) {
    current->errno = 0;
  }
  return file;
}

int proc_file_set_flags(struct proc_info_t* proc, int fd, uint32_t flags) {
  if(proc == NULL || fd < 0 || fd >= PROC_MAX_FILES) {
    set_errno(EBADF);
    return -EBADF;
  }
  if(proc->files[fd].file == NULL) {
    set_errno(EBADF);
    return -EBADF;
  }
  proc->files[fd].flags = flags;
  if(current) {
    current->errno = 0;
  }
  return 0;
}

int proc_file_close(struct proc_info_t* proc, int fd) {
  if(proc == NULL || fd < 0 || fd >= PROC_MAX_FILES) {
    set_errno(EBADF);
    return -EBADF;
  }
  file_t* file = proc->files[fd].file;
  if(file == NULL) {
    set_errno(EBADF);
    return -EBADF;
  }
  proc->files[fd].file = NULL;
  proc->files[fd].flags = 0;
  file_unref(file);
  if(current) {
    current->errno = 0;
  }
  return 0;
}

int proc_file_dup(struct proc_info_t* proc, int oldfd, int newfd, bool cloexec) {
  if(proc == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if(oldfd < 0 || oldfd >= PROC_MAX_FILES) {
    set_errno(EBADF);
    return -EBADF;
  }
  file_t* file = proc->files[oldfd].file;
  if(file == NULL) {
    set_errno(EBADF);
    return -EBADF;
  }

  int target_fd = newfd;
  if(newfd < 0) {
    target_fd = find_free_fd(proc, 0);
    if(target_fd < 0) {
      set_errno(-target_fd);
      return target_fd;
    }
  } else if(newfd >= PROC_MAX_FILES) {
    set_errno(EBADF);
    return -EBADF;
  }

  if(target_fd == oldfd) {
    if(cloexec) {
      proc->files[target_fd].flags |= FD_FLAG_CLOEXEC;
    }
    if(current) {
      current->errno = 0;
    }
    return target_fd;
  }

  if(proc->files[target_fd].file != NULL) {
    file_unref(proc->files[target_fd].file);
  }

  file_ref(file);
  proc->files[target_fd].file = file;
  uint32_t flags = proc->files[oldfd].flags;
  if(cloexec) {
    flags |= FD_FLAG_CLOEXEC;
  }
  proc->files[target_fd].flags = flags;

  if(current) {
    current->errno = 0;
  }
  return target_fd;
}

file_descriptor_t fd_get(int fd) {
  if(fd < 0 || fd >= PROC_MAX_FILES) {
    return NULL;
  }
  return kernel_process_info.files[fd].file;
}

static int64_t serial_write_impl(file_t* file, const void* buffer, size_t length) {
  (void)file;
  const char* data = (const char*)buffer;
  for(size_t idx = 0; idx < length; idx++) {
    serial_putchar(data[idx]);
  }
  return (int64_t)length;
}

static int serial_close_noop(file_t* file) {
  (void)file;
  return 0;
}

static int64_t framebuffer_write_impl(file_t* file, const void* buffer, size_t length) {
  (void)file;
  const char* text = (const char*)buffer;
  for(size_t idx = 0; idx < length; idx++) {
    fb_putchar(text[idx]);
  }
  return (int64_t)length;
}

static const file_ops_t serial_file_ops = {
  .read = NULL,
  .write = serial_write_impl,
  .close = serial_close_noop,
  .seek = NULL,
};

static const file_ops_t framebuffer_file_ops = {
  .read = NULL,
  .write = framebuffer_write_impl,
  .close = serial_close_noop,
  .seek = NULL,
};

static int console_char_open(char_device_t* device, uint32_t mode, file_t** out_file) {
  (void)device;
  (void)mode;

  file_t* file = console_device_open();
  if(file == NULL) {
    return -ENOMEM;
  }
  *out_file = file;
  return 0;
}

static int serial_char_open(char_device_t* device, uint32_t mode, file_t** out_file) {
  (void)device;
  if((mode & FILE_MODE_WRITE) == 0) {
    return -EACCES;
  }

  file_t* file = file_create(&serial_file_ops, NULL, FILE_MODE_WRITE);
  if(file == NULL) {
    return -ENOMEM;
  }
  *out_file = file;
  return 0;
}

static int framebuffer_char_open(char_device_t* device, uint32_t mode, file_t** out_file) {
  (void)device;
  if((mode & FILE_MODE_WRITE) == 0) {
    return -EACCES;
  }

  file_t* file = file_create(&framebuffer_file_ops, NULL, FILE_MODE_WRITE);
  if(file == NULL) {
    return -ENOMEM;
  }
  *out_file = file;
  return 0;
}

static int keyboard_char_open(char_device_t* device, uint32_t mode, file_t** out_file) {
  (void)device;
  if((mode & FILE_MODE_READ) == 0) {
    return -EACCES;
  }

  file_t* file = keyboard_device_open();
  if(file == NULL) {
    return -ENOMEM;
  }
  *out_file = file;
  return 0;
}

static const char_device_ops_t console_char_ops = { .open = console_char_open };
static const char_device_ops_t serial_char_ops = { .open = serial_char_open };
static const char_device_ops_t framebuffer_char_ops = { .open = framebuffer_char_open };
static const char_device_ops_t keyboard_char_ops = { .open = keyboard_char_open };

static char_device_t console_char_device = {
  .name = "/dev/console",
  .supported_modes = FILE_MODE_READ | FILE_MODE_WRITE,
  .ops = &console_char_ops,
  .driver_ctx = NULL,
  .next = NULL,
};

static char_device_t serial_char_device = {
  .name = "/dev/ttyS0",
  .supported_modes = FILE_MODE_WRITE,
  .ops = &serial_char_ops,
  .driver_ctx = NULL,
  .next = NULL,
};

static char_device_t framebuffer_char_device = {
  .name = "/dev/fb/0",
  .supported_modes = FILE_MODE_WRITE,
  .ops = &framebuffer_char_ops,
  .driver_ctx = NULL,
  .next = NULL,
};

static char_device_t keyboard_char_device = {
  .name = "/dev/input/kbd",
  .supported_modes = FILE_MODE_READ,
  .ops = &keyboard_char_ops,
  .driver_ctx = NULL,
  .next = NULL,
};

#ifdef MENIOS_KERNEL
static void install_standard_streams(void) {
  proc_file_table_init(&kernel_process_info);

  tty_system_init();
  keyboard_device_init();

  file_t* tty_file = tty_device_open();
  if(tty_file != NULL) {
    proc_file_install_at(&kernel_process_info, FD_STDIN, tty_file, 0);
    file_ref(tty_file);
    proc_file_install_at(&kernel_process_info, FD_STDOUT, tty_file, 0);
    file_ref(tty_file);
    proc_file_install_at(&kernel_process_info, FD_STDERR, tty_file, 0);
    file_unref(tty_file);
  }
}
#endif

void file_system_init(void) {
#ifdef MENIOS_KERNEL
  char_device_system_init();
  if(!char_device_register(vga_text_char_device())) {
    serial_printf("file_system_init: failed to register /dev/vga/0\n");
  }
  if(!char_device_register(null_char_device())) {
    serial_printf("file_system_init: failed to register /dev/null\n");
  }
  if(!char_device_register(zero_char_device())) {
    serial_printf("file_system_init: failed to register /dev/zero\n");
  }
  if(!char_device_register(tty_char_device())) {
    serial_printf("file_system_init: failed to register /dev/tty0\n");
  }
  if(!char_device_register(&console_char_device)) {
    serial_printf("file_system_init: failed to register /dev/console\n");
  }
  if(!char_device_register(&serial_char_device)) {
    serial_printf("file_system_init: failed to register /dev/ttyS0\n");
  }
  if(!char_device_register(&framebuffer_char_device)) {
    serial_printf("file_system_init: failed to register /dev/fb/0\n");
  }
  if(!char_device_register(&keyboard_char_device)) {
    serial_printf("file_system_init: failed to register /dev/input/kbd\n");
  }
  install_standard_streams();
#endif
}

static struct proc_info_t* stream_owner(void) {
  return owning_proc();
}

#ifdef MENIOS_KERNEL
int dup2(int oldfd, int newfd) {
  return proc_file_dup(stream_owner(), oldfd, newfd, false);
}

FILE* fopen(const char* filename, const char* mode) {
  if(filename == NULL || mode == NULL) {
    set_errno(EINVAL);
    return NULL;
  }

  bool write = mode[0] == 'w' || mode[0] == 'a';
  bool read = mode[0] == 'r';
  bool update = false;
  for(const char* it = mode; *it != '\0'; ++it) {
    if(*it == '+') {
      update = true;
      break;
    }
  }

  if(update) {
    set_errno(ENOSYS);
    return NULL;
  }

  file_t* file = NULL;
  uint32_t file_mode = 0;
  if(read) {
    file_mode |= FILE_MODE_READ;
  }
  if(write) {
    file_mode |= FILE_MODE_WRITE;
  }

  int rc = char_device_open(filename, file_mode, &file);
  if(rc == 0) {
    // char device handled successfully
  } else if(rc != -ENODEV) {
    set_errno(-rc);
    return NULL;
  } else if(read && !write) {
    rc = vfs_open(filename, O_RDONLY, &file);
    if(rc < 0) {
      set_errno(-rc);
      return NULL;
    }
    file_mode = FILE_MODE_READ;
  } else {
    set_errno(ENOSYS);
    return NULL;
  }

  struct proc_info_t* proc = stream_owner();
  int fd = proc_file_install(proc, file, 0);
  file_unref(file);
  if(fd < 0) {
    set_errno(-fd);
    return NULL;
  }

  FILE* stream = kmalloc(sizeof(FILE));
  if(stream == NULL) {
    proc_file_close(proc, fd);
    set_errno(ENOMEM);
    return NULL;
  }
  (void)file_mode;
  stream->reserved = fd;
  return stream;
}

int fclose(FILE* stream) {
  if(stream == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }

  if(stream == &kernel_stdin_stream || stream == &kernel_stdout_stream || stream == &kernel_stderr_stream) {
    return -EBADF;
  }

  struct proc_info_t* proc = stream_owner();
  int fd = stream->reserved;
  int rc = proc_file_close(proc, fd);
  kfree(stream);
  return rc;
}

FILE* freopen(const char* filename, const char* mode, FILE* stream) {
  if(stream == NULL) {
    set_errno(EINVAL);
    return NULL;
  }

  FILE* new_stream = fopen(filename, mode);
  if(new_stream == NULL) {
    return NULL;
  }

  struct proc_info_t* proc = stream_owner();
  if(proc_file_dup(proc, new_stream->reserved, stream->reserved, false) < 0) {
    fclose(new_stream);
    return NULL;
  }

fclose(new_stream);
  return stream;
}
#endif
