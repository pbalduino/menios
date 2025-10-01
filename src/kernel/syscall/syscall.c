#include <errno.h>
#include <kernel/console.h>
#include <kernel/file.h>
#include <kernel/heap.h>
#include <kernel/mman.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/syscall.h>
#include <kernel/vfs.h>
#include <sys/fcntl.h>

#define SYSCALL_MAX 256

static uint64_t syscall_stub_unimplemented(syscall_frame_t* frame);
static uint64_t syscall_read_handler(syscall_frame_t* frame);
static uint64_t syscall_write_handler(syscall_frame_t* frame);
static uint64_t syscall_close_handler(syscall_frame_t* frame);
static uint64_t syscall_open_handler(syscall_frame_t* frame);
static uint64_t syscall_lseek_handler(syscall_frame_t* frame);
static uint64_t syscall_mmap_handler(syscall_frame_t* frame);
static uint64_t syscall_munmap_handler(syscall_frame_t* frame);
static uint64_t syscall_pipe_handler(syscall_frame_t* frame);
static uint64_t syscall_dup_handler(syscall_frame_t* frame);
static uint64_t syscall_dup2_handler(syscall_frame_t* frame);
static uint64_t syscall_fork_handler(syscall_frame_t* frame);
static uint64_t syscall_execve_handler(syscall_frame_t* frame);
static uint64_t syscall_yield_handler(syscall_frame_t* frame);
static uint64_t syscall_sleep_handler(syscall_frame_t* frame);
static uint64_t syscall_exit_handler(syscall_frame_t* frame);
static uint64_t syscall_fcntl_handler(syscall_frame_t* frame);

static syscall_handler_t syscall_table[SYSCALL_MAX];

#define SYSCALL_PATH_MAX 256
#define EXECVE_MAX_ARGS   64
#define EXECVE_MAX_ENVP   64
#define EXECVE_MAX_STRING 4096

static bool copy_user_string(const char* user_ptr, char* dest, size_t capacity) {
  if(current == NULL || user_ptr == NULL || dest == NULL || capacity == 0) {
    return false;
  }

  size_t copied = 0;
  while(copied < capacity) {
    if(!proc_user_buffer_accessible(current, user_ptr + copied, 1)) {
      return false;
    }

    char ch = user_ptr[copied];
    dest[copied++] = ch;
    if(ch == '\0') {
      return true;
    }
  }

  dest[capacity - 1] = '\0';
  return false;
}

static void free_string_vector(char** vector, size_t count) {
  if(vector == NULL) {
    return;
  }
  for(size_t i = 0; i < count; i++) {
    if(vector[i]) {
      kfree(vector[i]);
    }
  }
  kfree(vector);
}

static char* duplicate_user_string(const char* user_ptr) {
  if(user_ptr == NULL) {
    return NULL;
  }

  size_t capacity = 64;
  char* buffer = kmalloc(capacity);
  if(buffer == NULL) {
    return NULL;
  }

  size_t len = 0;
  while(true) {
    if(len >= EXECVE_MAX_STRING) {
      kfree(buffer);
      return NULL;
    }

    if(!proc_user_buffer_accessible(current, user_ptr + len, 1)) {
      kfree(buffer);
      return NULL;
    }

    char ch = user_ptr[len];
    if(len + 1 >= capacity) {
      size_t new_capacity = capacity * 2;
      char* resized = krealloc(buffer, new_capacity);
      if(resized == NULL) {
        kfree(buffer);
        return NULL;
      }
      buffer = resized;
      capacity = new_capacity;
    }

    buffer[len++] = ch;
    if(ch == '\0') {
      break;
    }
  }

  return buffer;
}

static bool clone_user_vector(const char* const* user_vec,
                              size_t max_entries,
                              char*** out_vec,
                              size_t* out_count,
                              int* err_out) {
  if(out_vec == NULL || out_count == NULL) {
    return false;
  }

  *out_vec = NULL;
  *out_count = 0;

  if(err_out) {
    *err_out = 0;
  }

  if(user_vec == NULL) {
    return true;
  }

  size_t capacity = 8;
  char** vector = kmalloc(capacity * sizeof(char*));
  if(vector == NULL) {
    return false;
  }

  size_t count = 0;
  bool success = false;

  for(size_t i = 0; i < max_entries; i++) {
    if(!proc_user_buffer_accessible(current, user_vec + i, sizeof(char*))) {
      if(err_out) {
        *err_out = -EFAULT;
      }
      goto out;
    }

    const char* entry = user_vec[i];
    if(entry == NULL) {
      success = true;
      break;
    }

    char* dup = duplicate_user_string(entry);
    if(dup == NULL) {
      if(err_out) {
        *err_out = -EFAULT;
      }
      goto out;
    }

    if(count == capacity) {
      size_t new_capacity = capacity * 2;
      char** resized = krealloc(vector, new_capacity * sizeof(char*));
      if(resized == NULL) {
        kfree(dup);
        if(err_out) {
          *err_out = -ENOMEM;
        }
        goto out;
      }
      vector = resized;
      capacity = new_capacity;
    }

    vector[count++] = dup;
  }

  if(!success) {
    if(proc_user_buffer_accessible(current, user_vec + max_entries, sizeof(char*)) &&
       user_vec[max_entries] == NULL) {
      success = true;
    } else if(err_out) {
      *err_out = -E2BIG;
    }
  }

out:
  if(success) {
    *out_vec = vector;
    *out_count = count;
    return true;
  }

  free_string_vector(vector, count);
  if(err_out && *err_out == 0) {
    *err_out = -EFAULT;
  }
  return false;
}

static void syscall_register(uint64_t number, syscall_handler_t handler) {
  if(number >= SYSCALL_MAX) {
    serial_printf("syscall_register: number %lu out of range\n", number);
    return;
  }
  syscall_table[number] = handler ? handler : syscall_stub_unimplemented;
}

void syscall_init(void) {
  for(size_t i = 0; i < SYSCALL_MAX; i++) {
    syscall_table[i] = syscall_stub_unimplemented;
  }

  syscall_register(SYS_READ, syscall_read_handler);
  syscall_register(SYS_WRITE, syscall_write_handler);
  syscall_register(SYS_OPEN, syscall_open_handler);
  syscall_register(SYS_CLOSE, syscall_close_handler);
  syscall_register(SYS_LSEEK, syscall_lseek_handler);
  syscall_register(SYS_MMAP, syscall_mmap_handler);
  syscall_register(SYS_MUNMAP, syscall_munmap_handler);
  syscall_register(SYS_PIPE, syscall_pipe_handler);
  syscall_register(SYS_DUP, syscall_dup_handler);
  syscall_register(SYS_DUP2, syscall_dup2_handler);
  syscall_register(SYS_FORK, syscall_fork_handler);
  syscall_register(SYS_EXECVE, syscall_execve_handler);
  syscall_register(SYS_YIELD, syscall_yield_handler);
  syscall_register(SYS_SLEEP, syscall_sleep_handler);
  syscall_register(SYS_EXIT, syscall_exit_handler);
  syscall_register(SYS_FCNTL, syscall_fcntl_handler);

  serial_printf("syscall_init: initialized dispatcher (INT 0x80)\n");
}

uint64_t syscall_dispatch(syscall_frame_t* frame) {
  uint64_t number = frame->rax;

  if(number < SYSCALL_MAX) {
    syscall_handler_t handler = syscall_table[number];
    if(handler) {
      uint64_t result = handler(frame);
      frame->rax = result;
      return result;
    }
  }

  frame->rax = (uint64_t)(-ENOSYS);
  return frame->rax;
}

static uint64_t syscall_stub_unimplemented(syscall_frame_t* frame) {
  serial_printf("syscall_stub_unimplemented: number %lu not implemented\n", frame->rax);
  return (uint64_t)(-ENOSYS);
}

static uint64_t syscall_read_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  void* buffer = (void*)frame->rsi;
  size_t length = (size_t)frame->rdx;
  file_t* file = proc_file_get(current, fd, NULL);
  if(file == NULL) {
    int err = current->errno ? current->errno : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  int64_t result = file_read(file, buffer, length);
  file_unref(file);
  frame->rax = (uint64_t)result;
  return frame->rax;
}

static uint64_t syscall_write_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  const void* buffer = (const void*)frame->rsi;
  size_t length = (size_t)frame->rdx;
  file_t* file = proc_file_get(current, fd, NULL);
  if(file == NULL) {
    int err = current->errno ? current->errno : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  int64_t result = file_write(file, buffer, length);
  file_unref(file);
  frame->rax = (uint64_t)result;
  return frame->rax;
}

static uint64_t syscall_close_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  int rc = proc_file_close(current, fd);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_open_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* path = (const char*)frame->rdi;
  int flags = (int)frame->rsi;
  (void)frame->rdx; // mode currently unused

  if(path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  uint32_t install_flags = 0;
  if(flags & O_CLOEXEC) {
    install_flags |= FD_FLAG_CLOEXEC;
  }

  file_t* file = NULL;
  int rc = vfs_open(path, flags, &file);
  if(rc < 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  int fd = proc_file_install(current, file, install_flags);
  file_unref(file);
  if(fd < 0) {
    frame->rax = (uint64_t)fd;
    return frame->rax;
  }

  frame->rax = (uint64_t)fd;
  return frame->rax;
}

static uint64_t syscall_lseek_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  int64_t offset = (int64_t)frame->rsi;
  int whence = (int)frame->rdx;

  file_t* file = proc_file_get(current, fd, NULL);
  if(file == NULL) {
    int err = current->errno ? current->errno : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  int64_t result = file_seek(file, offset, whence);
  file_unref(file);
  frame->rax = (uint64_t)result;
  return frame->rax;
}

static uint64_t syscall_mmap_handler(syscall_frame_t* frame) {
  void* addr = (void*)frame->rdi;
  size_t length = (size_t)frame->rsi;
  int prot = (int)frame->rdx;
  int flags = (int)frame->r10;
  int fd = (int)frame->r8;
  off_t offset = (off_t)frame->r9;

  void* result = kmmap(addr, length, prot, flags, fd, offset);
  if(result == MAP_FAILED) {
    int err = current ? current->errno : ENOMEM;
    if(err == 0) {
      err = ENOMEM;
    }
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  frame->rax = (uint64_t)result;
  return frame->rax;
}

static uint64_t syscall_munmap_handler(syscall_frame_t* frame) {
  void* addr = (void*)frame->rdi;
  size_t length = (size_t)frame->rsi;

  int rc = kmunmap(addr, length);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_pipe_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int* fds = (int*)frame->rdi;
  if(fds == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  file_t* read_end = NULL;
  file_t* write_end = NULL;

  int rc = pipe_create(&read_end, &write_end);
  if(rc < 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  int read_fd = proc_file_install(current, read_end, 0);
  if(read_fd < 0) {
    file_unref(read_end);
    file_unref(write_end);
    frame->rax = (uint64_t)read_fd;
    return frame->rax;
  }

  int write_fd = proc_file_install(current, write_end, 0);
  if(write_fd < 0) {
    proc_file_close(current, read_fd);
    file_unref(read_end);
    file_unref(write_end);
    frame->rax = (uint64_t)write_fd;
    return frame->rax;
  }

  file_unref(read_end);
  file_unref(write_end);

  fds[0] = read_fd;
  fds[1] = write_fd;
  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_dup_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int oldfd = (int)frame->rdi;
  int rc = proc_file_dup(current, oldfd, -1, false);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_dup2_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int oldfd = (int)frame->rdi;
  int newfd = (int)frame->rsi;
  int rc = proc_file_dup(current, oldfd, newfd, false);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_fork_handler(syscall_frame_t* frame) {
  int err = 0;
  proc_info_p child = proc_fork(current, frame, &err);
  if(child == NULL) {
    if(err == 0) {
      err = -ENOMEM;
    }
    frame->rax = (uint64_t)err;
    return frame->rax;
  }

  frame->rax = (uint64_t)child->pid;
  return frame->rax;
}

static uint64_t syscall_execve_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* user_path = (const char*)frame->rdi;
  if(user_path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char path[SYSCALL_PATH_MAX];
  if(!copy_user_string(user_path, path, sizeof(path))) {
    if(current) {
      current->errno = EFAULT;
    }
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char** argv = NULL;
  size_t argc = 0;
  int vector_err = 0;
  if(!clone_user_vector((const char* const*)frame->rsi,
                        EXECVE_MAX_ARGS,
                        &argv,
                        &argc,
                        &vector_err)) {
    free_string_vector(argv, argc);
    if(current) {
      current->errno = vector_err ? -vector_err : EFAULT;
    }
    frame->rax = (uint64_t)(vector_err ? vector_err : -EFAULT);
    return frame->rax;
  }

  char** envp = NULL;
  size_t envc = 0;
  if(!clone_user_vector((const char* const*)frame->rdx,
                        EXECVE_MAX_ENVP,
                        &envp,
                        &envc,
                        &vector_err)) {
    free_string_vector(argv, argc);
    if(current) {
      current->errno = vector_err ? -vector_err : EFAULT;
    }
    frame->rax = (uint64_t)(vector_err ? vector_err : -EFAULT);
    return frame->rax;
  }

  void* image = NULL;
  size_t size = 0;
  if(!vfs_read_all(path, &image, &size) || image == NULL || size == 0) {
    if(image != NULL) {
      kfree(image);
    }
    free_string_vector(argv, argc);
    free_string_vector(envp, envc);
    if(current) {
      current->errno = ENOENT;
    }
    frame->rax = (uint64_t)(-ENOENT);
    return frame->rax;
  }

  if(size > (32 * 1024 * 1024)) {
    kfree(image);
    free_string_vector(argv, argc);
    free_string_vector(envp, envc);
    if(current) {
      current->errno = EFBIG;
    }
    frame->rax = (uint64_t)(-EFBIG);
    return frame->rax;
  }

  proc_exec_args_t exec_args = {
    .argc = argc,
    .argv = argv,
    .envc = envc,
    .envp = envp,
  };

  int err = proc_exec_image(current, image, size, frame, &exec_args);
  kfree(image);
  free_string_vector(argv, argc);
  free_string_vector(envp, envc);
  frame->rax = (uint64_t)err;
  return frame->rax;
}

static uint64_t syscall_yield_handler(syscall_frame_t* frame) {
  proc_request_yield();
  proc_switch((void*)frame);
  frame->rax = 0;
  return 0;
}

static uint64_t syscall_sleep_handler(syscall_frame_t* frame) {
  uint64_t usec = frame->rdi;
  proc_request_sleep(usec);
  proc_switch((void*)frame);
  frame->rax = 0;
  return 0;
}

static uint64_t syscall_exit_handler(syscall_frame_t* frame) {
  int status = (int)frame->rdi;
  proc_exit(status);
  proc_switch((void*)frame);
  return frame->rax;
}

static uint64_t syscall_fcntl_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  int cmd = (int)frame->rsi;
  uint64_t arg = frame->rdx;

  uint32_t fd_flags = 0;
  file_t* file = proc_file_get(current, fd, &fd_flags);
  if(file == NULL) {
    int err = current->errno ? current->errno : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  file_unref(file);

  switch(cmd) {
    case F_GETFD: {
      int result = (fd_flags & FD_FLAG_CLOEXEC) ? FD_CLOEXEC : 0;
      frame->rax = (uint64_t)result;
      return frame->rax;
    }
    case F_SETFD: {
      uint32_t new_flags = fd_flags;
      if(arg & FD_CLOEXEC) {
        new_flags |= FD_FLAG_CLOEXEC;
      } else {
        new_flags &= ~FD_FLAG_CLOEXEC;
      }
      int rc = proc_file_set_flags(current, fd, new_flags);
      frame->rax = (uint64_t)rc;
      return frame->rax;
    }
    default:
      frame->rax = (uint64_t)(-ENOSYS);
      return frame->rax;
  }
}
