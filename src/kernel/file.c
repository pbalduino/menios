#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#include <kernel/condvar.h>
#include <kernel/file.h>
#include <kernel/fs/devfs/devfs.h>
#include <kernel/fs/core.h>
#include <kernel/fs/procfs/procfs.h>
#include <kernel/fs/tmpfs/tmpfs.h>
#include <kernel/framebuffer.h>
#include <kernel/mman.h>
#include <kernel/pmm.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>
#include <kernel/fs/vfs/vfs.h>
#include <menios/fb.h>
#ifdef MENIOS_KERNEL
#include <menios/stdio_internal.h>
#endif

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

static const file_ops_t serial_file_ops;
static const file_ops_t stdin_file_ops;

#ifdef MENIOS_KERNEL
static FILE kernel_stdin_stream = { .fd = FD_STDIN };
static FILE kernel_stdout_stream = { .fd = FD_STDOUT };
static FILE kernel_stderr_stream = { .fd = FD_STDERR };

FILE* stdin = &kernel_stdin_stream;
FILE* stdout = &kernel_stdout_stream;
FILE* stderr = &kernel_stderr_stream;
#endif

static file_t* serial_stdout_file = NULL;
static file_t* serial_stderr_file = NULL;
static file_t* stdin_stream_file  = NULL;

#define STDIN_BUFFER_SIZE 256

typedef struct stdin_ring_buffer_t {
  spinlock_t lock;
  kmutex_t   wait_lock;
  kcondvar_t waiters;
  uint8_t    data[STDIN_BUFFER_SIZE];
  size_t     head;
  size_t     tail;
} stdin_ring_buffer_t;

static stdin_ring_buffer_t stdin_buffer;
static bool stdin_initialized = false;

static const uintptr_t kernel_pointer_floor = 0xffff800000000000ull;
#define FILE_IO_BOUNCE_CHUNK (PAGE_SIZE * 4u)

static bool buffer_needs_user_copy(const void* buffer, size_t length) {
  if(buffer == NULL || length == 0) {
    return false;
  }
  if(current == NULL || !current->user_mode) {
    return false;
  }
  return (uintptr_t)buffer < kernel_pointer_floor;
}

static int64_t file_user_read(file_t* file, void* buffer, size_t length) {
  if(length == 0) {
    return 0;
  }

  if(file == NULL || file->ops == NULL || file->ops->read == NULL) {
    return -EBADF;
  }

  virt_addr_t dest = (virt_addr_t)(uintptr_t)buffer;

  size_t chunk = length < FILE_IO_BOUNCE_CHUNK ? length : FILE_IO_BOUNCE_CHUNK;
  if(chunk == 0) {
    chunk = length;
  }

  uint8_t* bounce = kmalloc(chunk);
  if(bounce == NULL) {
    return -ENOMEM;
  }

  size_t total = 0;
  int64_t result = 0;

  while(total < length) {
    size_t request = length - total;
    if(request > chunk) {
      request = chunk;
    }

    int64_t rc = file->ops->read(file, bounce, request);
    if(rc < 0) {
      result = rc;
      break;
    }
    if(rc == 0) {
      result = (int64_t)total;
      break;
    }

    size_t copied = (size_t)rc;
    if(!proc_user_copy_out(current, dest + total, bounce, copied)) {
      serial_printf("file_user_read: copy_out failed pid=%u dest=%lx bytes=%lu\n",
                    current ? current->pid : 0,
                    (unsigned long)(dest + total),
                    (unsigned long)copied);
      result = -EFAULT;
      break;
    }

    total += copied;
    if(copied < request) {
      result = (int64_t)total;
      break;
    }
  }

  if(result == 0 && total == length) {
    result = (int64_t)total;
  }

  kfree(bounce);
  return result;
}

static int64_t file_user_write(file_t* file, const void* buffer, size_t length) {
  if(length == 0) {
    return 0;
  }

  if(file == NULL || file->ops == NULL || file->ops->write == NULL) {
    return -EBADF;
  }

  virt_addr_t src = (virt_addr_t)(uintptr_t)buffer;
  if(!proc_user_touch_range(current, src, length, false)) {
    return -EFAULT;
  }

  size_t chunk = length < FILE_IO_BOUNCE_CHUNK ? length : FILE_IO_BOUNCE_CHUNK;
  if(chunk == 0) {
    chunk = length;
  }

  uint8_t* bounce = kmalloc(chunk);
  if(bounce == NULL) {
    return -ENOMEM;
  }

  size_t total = 0;
  int64_t result = 0;

  while(total < length) {
    size_t request = length - total;
    if(request > chunk) {
      request = chunk;
    }

    if(!proc_user_copy_in(current, bounce, src + total, request)) {
      result = -EFAULT;
      break;
    }

    int64_t rc = file->ops->write(file, bounce, request);
    if(rc < 0) {
      result = rc;
      break;
    }
    if(rc == 0) {
      result = (int64_t)total;
      break;
    }

    size_t written_chunk = (size_t)rc;
    total += written_chunk;
    if(written_chunk < request) {
      result = (int64_t)total;
      break;
    }
  }

  if(result == 0 && total == length) {
    result = (int64_t)total;
  }

  kfree(bounce);
  return result;
}

static struct proc_info_t* owning_proc(void) {
  if(current != NULL) {
    return current;
  }
  return &kernel_process_info;
}

static inline void set_errno(int err) {
  if(current) {
    current->err_no = err;
  }
}

static inline void stdin_buffer_init(void) {
  if(stdin_initialized) {
    return;
  }
  spinlock_init(&stdin_buffer.lock);
  kmutex_init(&stdin_buffer.wait_lock);
  kcondvar_init(&stdin_buffer.waiters);
  stdin_buffer.head = 0;
  stdin_buffer.tail = 0;
  stdin_initialized = true;
}

static bool stdin_buffer_pop(uint8_t* ch) {
  bool result = false;
  spinlock_lock(&stdin_buffer.lock);
  if(stdin_buffer.head != stdin_buffer.tail) {
    *ch = stdin_buffer.data[stdin_buffer.tail];
    stdin_buffer.tail = (stdin_buffer.tail + 1) % STDIN_BUFFER_SIZE;
    result = true;
  }
  spinlock_unlock(&stdin_buffer.lock);
  return result;
}

static bool stdin_buffer_push(uint8_t ch) {
  bool was_empty;
  spinlock_lock(&stdin_buffer.lock);
  was_empty = (stdin_buffer.head == stdin_buffer.tail);
  size_t next = (stdin_buffer.head + 1) % STDIN_BUFFER_SIZE;
  if(next == stdin_buffer.tail) {
    stdin_buffer.tail = (stdin_buffer.tail + 1) % STDIN_BUFFER_SIZE;
  }
  stdin_buffer.data[stdin_buffer.head] = ch;
  stdin_buffer.head = next;
  spinlock_unlock(&stdin_buffer.lock);
  return was_empty;
}

bool stdin_try_pop(uint8_t* ch) {
  if(ch == NULL) {
    return false;
  }
  return stdin_buffer_pop(ch);
}

static int64_t stdin_read_impl(file_t* file, void* buffer, size_t length) {
  (void)file;

  if(buffer == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }

  if(length == 0) {
    return 0;
  }

  uint8_t* out = (uint8_t*)buffer;
  size_t total = 0;

  while(total < length) {
    uint8_t ch = 0;
    if(stdin_buffer_pop(&ch)) {
      out[total++] = ch;
      continue;
    }

    if(total > 0) {
      break;
    }

    kmutex_lock(&stdin_buffer.wait_lock);
    for(;;) {
      if(stdin_buffer_pop(&ch)) {
        kmutex_unlock(&stdin_buffer.wait_lock);
        out[total++] = ch;
        break;
      }
      kcondvar_wait(&stdin_buffer.waiters, &stdin_buffer.wait_lock);
    }
  }

  return (int64_t)total;
}

void stdin_enqueue_char(uint8_t ch) {
  if(!stdin_initialized) {
    stdin_buffer_init();
  }
  (void)stdin_buffer_push(ch);
  kcondvar_signal(&stdin_buffer.waiters);
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
    serial_printf("file_read: denied file=%p mode=0x%x ops=%p\n",
                  (void*)file,
                  file ? file->mode : 0u,
                  file ? (void*)file->ops : NULL);
    set_errno(EBADF);
    return -EBADF;
  }
  int64_t result;
  if(buffer_needs_user_copy(buffer, length)) {
    result = file_user_read(file, buffer, length);
  } else {
    result = file->ops->read(file, buffer, length);
  }
  if(result < 0) {
    set_errno((int)-result);
  } else if(current) {
    current->err_no = 0;
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
  int64_t result;
  if(buffer_needs_user_copy(buffer, length)) {
    result = file_user_write(file, buffer, length);
  } else {
    result = file->ops->write(file, buffer, length);
  }
  if(result < 0) {
    set_errno((int)-result);
  } else if(current) {
    current->err_no = 0;
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
    current->err_no = 0;
  }
  return result;
}

int file_ioctl(file_t* file, unsigned long request, void* argp) {
  if(file == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if(file->ops == NULL || file->ops->ioctl == NULL) {
    set_errno(ENOTTY);
    return -ENOTTY;
  }

  int rc = file->ops->ioctl(file, request, argp);
  if(rc < 0) {
    set_errno(-rc);
  } else if(current) {
    current->err_no = 0;
  }
  return rc;
}

int file_mmap(file_t* file,
              const file_mmap_request_t* request,
              file_mmap_result_t* result) {
  if(file == NULL || request == NULL || result == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if(file->ops == NULL || file->ops->mmap == NULL) {
    set_errno(ENOSYS);
    return -ENOSYS;
  }

  int rc = file->ops->mmap(file, request, result);
  if(rc < 0) {
    set_errno(-rc);
  } else if(current) {
    current->err_no = 0;
  }
  return rc;
}

int file_chmod(file_t* file, mode_t mode) {
  if(file == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if(file->ops == NULL || file->ops->chmod == NULL) {
    set_errno(ENOSYS);
    return -ENOSYS;
  }

  int rc = file->ops->chmod(file, mode);
  if(rc < 0) {
    set_errno(-rc);
  } else if(current) {
    current->err_no = 0;
  }
  return rc;
}

int file_utimens(file_t* file, const struct timespec times[2]) {
  if(file == NULL) {
    set_errno(EINVAL);
    return -EINVAL;
  }
  if(file->ops == NULL || file->ops->utimens == NULL) {
    set_errno(ENOSYS);
    return -ENOSYS;
  }

  int rc = file->ops->utimens(file, times);
  if(rc < 0) {
    set_errno(-rc);
  } else if(current) {
    current->err_no = 0;
  }
  return rc;
}

void proc_file_table_init(struct proc_info_t* proc) {
  if(proc == NULL) {
    return;
  }
  for(size_t fd = 0; fd < PROC_MAX_FILES; fd++) {
    proc->files[fd].file = NULL;
    proc->files[fd].flags = 0;
  }
  if(proc->cwd_len == 0 || proc->cwd[0] == '\0') {
    proc->cwd[0] = '/';
    proc->cwd[1] = '\0';
    proc->cwd_len = 1;
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
    current->err_no = 0;
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
    current->err_no = 0;
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
    current->err_no = 0;
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
    current->err_no = 0;
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
      current->err_no = 0;
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
    current->err_no = 0;
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

static int64_t framebuffer_console_write_impl(file_t* file, const void* buffer, size_t length) {
  (void)file;
  const char* text = (const char*)buffer;
  for(size_t idx = 0; idx < length; idx++) {
    fb_putchar(text[idx]);
  }
  return (int64_t)length;
}

static int generic_char_stat(file_t* file, struct stat* out_stat) {
  if(out_stat == NULL) {
    return -EINVAL;
  }
  fs_path_info_t info;
  memset(&info, 0, sizeof(info));
  info.block_size = 4096;
  info.size = 0;
  info.is_directory = false;
  info.has_mode = true;
  info.inode = (uint64_t)(uintptr_t)file;

  mode_t perms = 0;
  if(file == NULL || (file->mode & FILE_MODE_READ)) {
    perms |= 0444;
  }
  if(file == NULL || (file->mode & FILE_MODE_WRITE)) {
    perms |= 0222;
  }
  if(perms == 0) {
    perms = 0600;
  }
  info.mode = S_IFCHR | perms;
  info.is_read_only = (file != NULL && (file->mode & FILE_MODE_WRITE) == 0);

  fs_path_info_to_stat(&info, out_stat);
  if(file != NULL) {
    char_device_t* device = (char_device_t*)file->private_data;
    if(device != NULL) {
      out_stat->st_rdev = device->dev;
    } else {
      out_stat->st_rdev = 0;
    }
  } else {
    out_stat->st_rdev = 0;
  }
  return 0;
}

static size_t align_up_size(size_t value) {
  if(value == 0) {
    return PAGE_SIZE;
  }
  size_t remainder = value % PAGE_SIZE;
  if(remainder == 0) {
    return value;
  }
  return value + (PAGE_SIZE - remainder);
}

static int framebuffer_ioctl_impl(file_t* file, unsigned long request, void* argp) {
  (void)file;
  switch(request) {
    case MENIOS_FB_IOCTL_GET_INFO: {
      if(argp == NULL) {
        return -EINVAL;
      }
      framebuffer_geometry_t geo;
      fb_get_geometry(&geo);
      menios_fb_info_t info = {
        .width = geo.width,
        .height = geo.height,
        .pitch = geo.pitch,
        .bpp = geo.bpp,
        .reserved = 0,
      };
      if(current != NULL && !proc_user_buffer_accessible(current, argp, sizeof(info))) {
        return -EFAULT;
      }
      memcpy(argp, &info, sizeof(info));
      return 0;
    }
    case MENIOS_FB_IOCTL_FLUSH: {
      uint32_t pid = current ? current->pid : (uint32_t)-1;
      if(fb_backbuffer_available() && (!fb_has_owner() || fb_is_owner(pid))) {
        fb_flush_backbuffer();
      }
      return 0;
    }
    case MENIOS_FB_IOCTL_SET_MODE: {
      if(argp == NULL) {
        return -EINVAL;
      }
      menios_fb_mode_request_t req;
      if(current != NULL && !proc_user_buffer_accessible(current, argp, sizeof(req))) {
        return -EFAULT;
      }
      memcpy(&req, argp, sizeof(req));
      if(req.width == 0 || req.height == 0) {
        return -EINVAL;
      }
      framebuffer_geometry_t geo;
      fb_get_geometry(&geo);
      uint16_t req_bpp = req.bpp ? req.bpp : (uint16_t)geo.bpp;
      bool restoring_boot = fb_is_boot_mode(req.width, req.height, req_bpp);
      uint32_t pid = current ? current->pid : (uint32_t)-1;
      if(!restoring_boot && !fb_is_owner(pid)) {
        return -EPERM;
      }
      if(!fb_set_mode(req.width, req.height, req_bpp)) {
        return -EINVAL;
      }
      if(fb_backbuffer_available()) {
        fb_flush_backbuffer();
      }
      return 0;
    }
    case MENIOS_FB_IOCTL_ENUM_MODES: {
      if(argp == NULL) {
        return -EINVAL;
      }
      menios_fb_modes_request_t req;
      if(current != NULL && !proc_user_buffer_accessible(current, argp, sizeof(req))) {
        return -EFAULT;
      }
      memcpy(&req, argp, sizeof(req));

      uint64_t total = fb_mode_count_total();

      uint64_t to_copy = req.capacity < total ? req.capacity : total;
      if(to_copy > 0) {
        if(req.modes == NULL) {
          return -EINVAL;
        }
        size_t bytes = to_copy * sizeof(menios_fb_mode_t);
        if(current != NULL && !proc_user_buffer_accessible(current, req.modes, bytes)) {
          return -EFAULT;
        }

        menios_fb_mode_t mode_info;
        menios_fb_mode_t* user_modes = req.modes;

        framebuffer_mode_info_t info;
        for(uint64_t i = 0; i < to_copy; i++) {
          if(!fb_mode_info(i, &info)) {
            break;
          }
          mode_info.width = info.width;
          mode_info.height = info.height;
          mode_info.pitch = info.pitch;
          mode_info.bpp = info.bpp;
          mode_info.reserved = 0;
          memcpy(user_modes + i, &mode_info, sizeof(mode_info));
        }
      }

      req.written = total;
      if(current != NULL && !proc_user_buffer_accessible(current, argp, sizeof(req))) {
        return -EFAULT;
      }
      memcpy(argp, &req, sizeof(req));
      return 0;
    }
    case MENIOS_FB_IOCTL_ACQUIRE: {
      if(current == NULL) {
        return -EPERM;
      }
      if(fb_acquire_owner(current->pid)) {
        return 0;
      }
      return -EBUSY;
    }
    case MENIOS_FB_IOCTL_RELEASE: {
      if(current == NULL) {
        return -EPERM;
      }
      if(!fb_has_owner()) {
        return 0;
      }
      if(!fb_release_owner(current->pid)) {
        return -EPERM;
      }
      return 0;
    }
    default:
      return -ENOTTY;
  }
}

static int framebuffer_mmap_impl(file_t* file,
                                 const file_mmap_request_t* request,
                                 file_mmap_result_t* result) {
  (void)file;
  if(request == NULL || result == NULL) {
    return -EINVAL;
  }

  if(request->offset != 0) {
    return -EINVAL;
  }

  uint32_t pid = current ? current->pid : (uint32_t)-1;
  if(fb_has_owner() && !fb_is_owner(pid)) {
    return -EPERM;
  }

  framebuffer_geometry_t geo;
  fb_get_geometry(&geo);
  if(geo.width == 0 || geo.height == 0 || geo.pitch == 0) {
    return -ENODEV;
  }

  size_t fb_size = fb_buffer_size();
  if(fb_size == 0) {
    fb_size = geo.pitch * geo.height;
  }
  size_t fb_size_aligned = align_up_size(fb_size);

  if(request->length == 0) {
    return -EINVAL;
  }
  size_t map_length = request->length;
  if(map_length > fb_size) {
    map_length = fb_size;
  }
  size_t aligned_length = align_up_size(map_length);
  if(aligned_length > fb_size_aligned) {
    aligned_length = fb_size_aligned;
  }

  phys_addr_t phys = fb_backbuffer_available() ? fb_backbuffer_physical()
                                               : fb_physical_address();
  if(phys == PHYS_ADDR_INVALID || phys == 0) {
    return -ENODEV;
  }

  result->phys_addr = phys;
  result->length = aligned_length;
  result->writable = true;
  return 0;
}

static const file_ops_t serial_file_ops = {
  .read = NULL,
  .write = serial_write_impl,
  .close = serial_close_noop,
  .seek = NULL,
  .ioctl = NULL,
  .mmap = NULL,
  .stat = generic_char_stat,
  .chmod = NULL,
  .utimens = NULL,
};

static const file_ops_t framebuffer_console_file_ops = {
  .read = NULL,
  .write = framebuffer_console_write_impl,
  .close = serial_close_noop,
  .seek = NULL,
  .ioctl = framebuffer_ioctl_impl,
  .mmap = framebuffer_mmap_impl,
  .stat = generic_char_stat,
  .chmod = NULL,
  .utimens = NULL,
};

static const file_ops_t framebuffer_device_file_ops = {
  .read = NULL,
  .write = NULL,
  .close = serial_close_noop,
  .seek = NULL,
  .ioctl = framebuffer_ioctl_impl,
  .mmap = framebuffer_mmap_impl,
  .stat = generic_char_stat,
  .chmod = NULL,
  .utimens = NULL,
};

static const file_ops_t stdin_file_ops = {
  .read = stdin_read_impl,
  .write = NULL,
  .close = serial_close_noop,
  .seek = NULL,
  .ioctl = NULL,
  .mmap = NULL,
  .stat = generic_char_stat,
  .chmod = NULL,
  .utimens = NULL,
};

static int64_t tty_write_impl(file_t* file, const void* buffer, size_t length) {
  (void)file;
  if(buffer == NULL) {
    return -EINVAL;
  }
  serial_write_impl(NULL, buffer, length);
  framebuffer_console_write_impl(NULL, buffer, length);
  return (int64_t)length;
}

static int tty_ioctl_impl(file_t* file, unsigned long request, void* argp) {
  (void)file;
  switch(request) {
    case TIOCGWINSZ: {
      if(argp == NULL) {
        return -EINVAL;
      }
      if(current != NULL && !proc_user_buffer_accessible(current, argp, sizeof(struct winsize))) {
        return -EFAULT;
      }
      struct winsize ws = {
        .ws_row = 25,
        .ws_col = 80,
        .ws_xpixel = 0,
        .ws_ypixel = 0,
      };
      memcpy(argp, &ws, sizeof(ws));
      return 0;
    }
    default:
      return -ENOTTY;
  }
}

static const file_ops_t tty_console_file_ops = {
  .read = stdin_read_impl,
  .write = tty_write_impl,
  .close = serial_close_noop,
  .seek = NULL,
  .ioctl = tty_ioctl_impl,
  .mmap = NULL,
  .stat = generic_char_stat,
  .chmod = NULL,
  .utimens = NULL,
};

#ifdef MENIOS_KERNEL
static void install_standard_streams(void) {
  proc_file_table_init(&kernel_process_info);

  stdin_buffer_init();

  stdin_stream_file = file_create(&stdin_file_ops, NULL, FILE_MODE_READ);
  if(stdin_stream_file != NULL) {
    proc_file_install_at(&kernel_process_info, FD_STDIN, stdin_stream_file, 0);
    file_unref(stdin_stream_file);
  }

  serial_stdout_file = file_create(&serial_file_ops, NULL, FILE_MODE_WRITE);
  if(serial_stdout_file != NULL) {
    proc_file_install_at(&kernel_process_info, FD_STDOUT, serial_stdout_file, 0);
    file_unref(serial_stdout_file);
  }

  serial_stderr_file = file_create(&serial_file_ops, NULL, FILE_MODE_WRITE);
  if(serial_stderr_file != NULL) {
    proc_file_install_at(&kernel_process_info, FD_STDERR, serial_stderr_file, 0);
    file_unref(serial_stderr_file);
  }
}
#endif

void file_system_init(void) {
#ifdef MENIOS_KERNEL
  install_standard_streams();
  char_device_system_init();
  if(!devfs_mount()) {
    serial_printf("file_system_init: failed to mount devfs\n");
  }
  if(!tmpfs_mount()) {
    serial_printf("file_system_init: failed to mount tmpfs\n");
  }
  if(!procfs_mount()) {
    serial_printf("file_system_init: failed to mount procfs\n");
  }
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

  if(strcmp(filename, "/dev/ttyS0") == 0 && write) {
    file = file_create(&serial_file_ops, NULL, FILE_MODE_WRITE);
    file_mode = FILE_MODE_WRITE;
  } else if(strcmp(filename, "/dev/console") == 0 && write) {
    file = file_create_framebuffer_console_file();
    file_mode = FILE_MODE_WRITE;
  } else if(strcmp(filename, "/dev/fb/0") == 0 && write) {
    file = file_create(&framebuffer_console_file_ops, NULL, FILE_MODE_WRITE);
    file_mode = FILE_MODE_WRITE;
  } else if(strcmp(filename, "/dev/fb0") == 0 && (read || write)) {
    file = file_create_framebuffer_device_file();
    file_mode = FILE_MODE_READ | FILE_MODE_WRITE;
  } else if(read && !write) {
    int rc = vfs_open(filename, O_RDONLY, &file);
    if(rc < 0) {
      set_errno(-rc);
      return NULL;
    }
    file_mode = FILE_MODE_READ;
  } else {
    set_errno(ENOSYS);
    return NULL;
  }

  if(file == NULL) {
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
  memset(stream, 0, sizeof(FILE));
  stream->fd = fd;
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
  int fd = stream->fd;
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
  if(proc_file_dup(proc, new_stream->fd, stream->fd, false) < 0) {
    fclose(new_stream);
    return NULL;
  }

fclose(new_stream);
  return stream;
}
#endif

file_t* file_create_serial_console_file(void) {
  return file_create(&serial_file_ops, NULL, FILE_MODE_WRITE);
}

file_t* file_create_framebuffer_console_file(void) {
  return file_create(&framebuffer_console_file_ops, NULL, FILE_MODE_WRITE);
}

file_t* file_create_framebuffer_device_file(void) {
  return file_create(&framebuffer_device_file_ops, NULL, FILE_MODE_READ | FILE_MODE_WRITE);
}

file_t* file_create_tty_console_file(void) {
  stdin_buffer_init();
  return file_create(&tty_console_file_ops, NULL, FILE_MODE_READ | FILE_MODE_WRITE);
}
