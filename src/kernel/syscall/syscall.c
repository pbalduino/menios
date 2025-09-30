#include <errno.h>
#include <kernel/console.h>
#include <kernel/file.h>
#include <kernel/framebuffer.h>
#include <kernel/mman.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/syscall.h>
#include <kernel/vfs.h>
#include <uapi/signal.h>
#include <sys/fcntl.h>
#include <string.h>

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
static uint64_t syscall_fb_getinfo_handler(syscall_frame_t* frame);
static uint64_t syscall_fb_map_handler(syscall_frame_t* frame);
static uint64_t syscall_fb_flip_handler(syscall_frame_t* frame);
static uint64_t syscall_kill_handler(syscall_frame_t* frame);
static uint64_t syscall_signal_handler(syscall_frame_t* frame);
static uint64_t syscall_sigreturn_handler(syscall_frame_t* frame);
static uint64_t syscall_sigaction_handler(syscall_frame_t* frame);
static uint64_t syscall_sigprocmask_handler(syscall_frame_t* frame);
static uint64_t syscall_waitpid_handler(syscall_frame_t* frame);
static uint64_t syscall_getcwd_handler(syscall_frame_t* frame);
static uint64_t syscall_chdir_handler(syscall_frame_t* frame);
static uint64_t syscall_getenv_handler(syscall_frame_t* frame);
static uint64_t syscall_setenv_handler(syscall_frame_t* frame);
static uint64_t syscall_unsetenv_handler(syscall_frame_t* frame);

static syscall_handler_t syscall_table[SYSCALL_MAX];

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
  syscall_register(SYS_KILL, syscall_kill_handler);
  syscall_register(SYS_SIGNAL, syscall_signal_handler);
  syscall_register(SYS_SIGRETURN, syscall_sigreturn_handler);
  syscall_register(SYS_SIGACTION, syscall_sigaction_handler);
  syscall_register(SYS_SIGPROCMASK, syscall_sigprocmask_handler);
  syscall_register(SYS_WAITPID, syscall_waitpid_handler);
  syscall_register(SYS_GETCWD, syscall_getcwd_handler);
  syscall_register(SYS_CHDIR, syscall_chdir_handler);
  syscall_register(SYS_GETENV, syscall_getenv_handler);
  syscall_register(SYS_SETENV, syscall_setenv_handler);
  syscall_register(SYS_UNSETENV, syscall_unsetenv_handler);
  syscall_register(SYS_FCNTL, syscall_fcntl_handler);
  syscall_register(SYS_FB_GETINFO, syscall_fb_getinfo_handler);
  syscall_register(SYS_FB_MAP, syscall_fb_map_handler);
  syscall_register(SYS_FB_FLIP, syscall_fb_flip_handler);

  serial_printf("syscall_init: initialized dispatcher (INT 0x80)\n");
}

uint64_t syscall_dispatch(syscall_frame_t* frame) {
  uint64_t number = frame->rax;

  if(number < SYSCALL_MAX) {
    syscall_handler_t handler = syscall_table[number];
    if(handler) {
      uint64_t result = handler(frame);
      frame->rax = result;
      if(current != NULL) {
        proc_process_pending_signals((cpu_state_p)frame);
      }
      return frame->rax;
    }
  }

  frame->rax = (uint64_t)(-ENOSYS);
  if(current != NULL) {
    proc_process_pending_signals((cpu_state_p)frame);
  }
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
  const uint8_t* image = (const uint8_t*)frame->rdi;
  size_t size = (size_t)frame->rsi;

  if(image == NULL || size == 0 || size > (32 * 1024 * 1024)) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int err = proc_exec_image(current, image, size, frame);
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

static uint64_t syscall_fb_getinfo_handler(syscall_frame_t* frame) {
  if(current == NULL || !current->user_mode) {
    frame->rax = (uint64_t)(-EPERM);
    return frame->rax;
  }

  fb_mode_info_t info = {0};
  fb_get_info(&info);
  if(info.buffer_size == 0) {
    frame->rax = (uint64_t)(-ENODEV);
    return frame->rax;
  }

  void* user_ptr = (void*)frame->rdi;
  if(user_ptr == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(!proc_user_buffer_accessible(current, user_ptr, sizeof(info), true)) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  memcpy(user_ptr, &info, sizeof(info));
  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_fb_map_handler(syscall_frame_t* frame) {
  (void)frame;

  if(current == NULL || !current->user_mode) {
    frame->rax = (uint64_t)(-EPERM);
    return frame->rax;
  }

  fb_mode_info_t info = {0};
  fb_get_info(&info);
  if(info.buffer_size == 0) {
    frame->rax = (uint64_t)(-ENODEV);
    return frame->rax;
  }

  if(current->fb_map_base != NULL && current->fb_map_size == info.buffer_size) {
    fb_copy_frontbuffer(current->fb_map_base, current->fb_map_size);
    frame->rax = (uint64_t)current->fb_map_base;
    return frame->rax;
  }

  void* mapped = kmmap(NULL,
                       (size_t)info.buffer_size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1,
                       0);
  if(mapped == MAP_FAILED) {
    int err = current->errno ? current->errno : ENOMEM;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  current->fb_map_base = mapped;
  current->fb_map_size = (size_t)info.buffer_size;
  current->fb_map_flags = 0;
  fb_copy_frontbuffer(mapped, current->fb_map_size);

  frame->rax = (uint64_t)mapped;
  return frame->rax;
}

static uint64_t syscall_fb_flip_handler(syscall_frame_t* frame) {
  if(current == NULL || !current->user_mode) {
    frame->rax = (uint64_t)(-EPERM);
    return frame->rax;
  }

  void* buffer = (void*)frame->rdi;
  uint64_t flags = frame->rsi;
  (void)flags;

  if(buffer == NULL) {
    buffer = current->fb_map_base;
  }

  if(buffer == NULL || current->fb_map_base == NULL || current->fb_map_size == 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(buffer != current->fb_map_base) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(!proc_user_buffer_accessible(current, buffer, current->fb_map_size, false)) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  if(!fb_present_user_buffer(buffer, current->fb_map_size)) {
    frame->rax = (uint64_t)(-EIO);
    return frame->rax;
  }

  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_kill_handler(syscall_frame_t* frame) {
  int pid = (int)frame->rdi;
  int sig = (int)frame->rsi;

  if(pid <= 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  proc_info_p target = proc_find((uint32_t)pid);
  if(target == NULL) {
    frame->rax = (uint64_t)(-ESRCH);
    return frame->rax;
  }

  int rc = proc_send_signal(target, sig);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_signal_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int sig = (int)frame->rdi;
  struct sigaction new_action;
  struct sigaction old_action;

  new_action.sa_handler = (sighandler_t)frame->rsi;
  new_action.sa_mask = 0;
  new_action.sa_flags = SA_RESTART;
  new_action.sa_restorer = (void(*)(void))frame->rdx;

  if(new_action.sa_handler == SIG_ERR) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int rc = proc_install_sigaction(current, sig, &new_action, &old_action);
  if(rc < 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  frame->rax = (uint64_t)old_action.sa_handler;
  return frame->rax;
}

static uint64_t syscall_sigreturn_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  signal_frame_t* user_frame = (signal_frame_t*)frame->rdi;
  if(user_frame == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(!proc_user_buffer_accessible(current, user_frame, sizeof(signal_frame_t), false)) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  signal_frame_t sigframe;
  memcpy(&sigframe, user_frame, sizeof(sigframe));

  memcpy(frame, &sigframe.saved_state, sizeof(cpu_state_t));
  if(current->cpu_state != NULL) {
    memcpy(current->cpu_state, &sigframe.saved_state, sizeof(cpu_state_t));
  }

  current->signal_mask = sigframe.saved_mask & ~SIG_UNBLOCKABLE_MASK;
  current->signal_depth = (uint8_t)sigframe.depth;

  proc_process_pending_signals((cpu_state_p)frame);

  return frame->rax;
}

static uint64_t syscall_sigaction_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int sig = (int)frame->rdi;
  const struct sigaction* user_act = (const struct sigaction*)frame->rsi;
  struct sigaction* user_old = (struct sigaction*)frame->rdx;

  struct sigaction act;
  struct sigaction old;
  struct sigaction* act_ptr = NULL;
  struct sigaction* old_ptr = NULL;

  if(user_act != NULL) {
    if(!proc_user_buffer_accessible(current, user_act, sizeof(struct sigaction), false)) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    memcpy(&act, user_act, sizeof(struct sigaction));
    act_ptr = &act;
  }

  if(user_old != NULL) {
    old_ptr = &old;
  }

  int rc = proc_install_sigaction(current, sig, act_ptr, old_ptr);
  if(rc < 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  if(user_old != NULL) {
    if(!proc_user_buffer_accessible(current, user_old, sizeof(struct sigaction), true)) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    memcpy(user_old, &old, sizeof(struct sigaction));
  }

  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_sigprocmask_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int how = (int)frame->rdi;
  const sigset_t* user_set = (const sigset_t*)frame->rsi;
  sigset_t* user_old = (sigset_t*)frame->rdx;

  sigset_t set = 0;
  sigset_t old = 0;
  sigset_t* old_ptr = NULL;

  if(user_set != NULL) {
    if(!proc_user_buffer_accessible(current, user_set, sizeof(sigset_t), false)) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    memcpy(&set, user_set, sizeof(sigset_t));
  }

  if(user_old != NULL) {
    old_ptr = &old;
  }

  int rc = 0;
  if(user_set != NULL) {
    rc = proc_update_signal_mask(current, how, set, old_ptr);
    if(rc < 0) {
      frame->rax = (uint64_t)rc;
      return frame->rax;
    }
  } else if(old_ptr != NULL) {
    *old_ptr = current->signal_mask;
  }

  if(user_old != NULL) {
    if(!proc_user_buffer_accessible(current, user_old, sizeof(sigset_t), true)) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    memcpy(user_old, &old, sizeof(sigset_t));
  }

  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_waitpid_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int pid = (int)frame->rdi;
  int* status_user = (int*)frame->rsi;
  int options = (int)frame->rdx;

  int status = 0;
  int* status_ptr = (status_user != NULL) ? &status : NULL;

  int rc = proc_waitpid(current, pid, options, status_ptr);
  if(rc < 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  if(status_user != NULL) {
    if(!proc_user_buffer_accessible(current, status_user, sizeof(int), true)) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    memcpy(status_user, &status, sizeof(int));
  }

  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_getcwd_handler(syscall_frame_t* frame) {
  char* buffer = (char*)frame->rdi;
  size_t size = (size_t)frame->rsi;

  if(buffer == NULL || size == 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  proc_info_p proc = current ? current : &kernel_process_info;
  size_t length = proc->cwd_len;
  if(length + 1 > size) {
    frame->rax = (uint64_t)(-ERANGE);
    return frame->rax;
  }

  memcpy(buffer, proc->cwd, length + 1);
  frame->rax = (uint64_t)(uintptr_t)buffer;
  return frame->rax;
}

static uint64_t syscall_chdir_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* path = (const char*)frame->rdi;
  if(path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char normalized[VFS_MAX_PATH];
  int rc = vfs_normalize_user_path(path, normalized, sizeof(normalized));
  if(rc < 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  rc = vfs_path_is_directory(normalized);
  if(rc < 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  proc_update_cwd(current, normalized);

  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_getenv_handler(syscall_frame_t* frame) {
  proc_info_p proc = current ? current : &kernel_process_info;

  const char* name = (const char*)frame->rdi;
  char* buffer = (char*)frame->rsi;
  size_t size = (size_t)frame->rdx;

  if(name == NULL || buffer == NULL || size == 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  size_t name_len = strnlen(name, PROC_ENV_NAME_MAX);
  if(name_len == 0 || name_len >= PROC_ENV_NAME_MAX) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }
  for(size_t i = 0; i < name_len; i++) {
    if(name[i] == '=') {
      frame->rax = (uint64_t)(-EINVAL);
      return frame->rax;
    }
  }

  if(!proc_user_buffer_accessible(proc, buffer, size, true)) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  const char* value = proc_env_get(proc, name);
  if(value == NULL) {
    frame->rax = 0;
    return frame->rax;
  }

  size_t value_len = strlen(value);
  if(value_len + 1 > size) {
    frame->rax = (uint64_t)(-ERANGE);
    return frame->rax;
  }

  memcpy(buffer, value, value_len + 1);
  frame->rax = (uint64_t)(uintptr_t)buffer;
  return frame->rax;
}

static uint64_t syscall_setenv_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* name = (const char*)frame->rdi;
  const char* value = (const char*)frame->rsi;
  int overwrite = (int)frame->rdx;

  if(name == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  size_t name_len = strnlen(name, PROC_ENV_NAME_MAX);
  if(name_len == 0 || name_len >= PROC_ENV_NAME_MAX) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }
  for(size_t i = 0; i < name_len; i++) {
    if(name[i] == '=') {
      frame->rax = (uint64_t)(-EINVAL);
      return frame->rax;
    }
  }

  char name_copy[PROC_ENV_NAME_MAX];
  memcpy(name_copy, name, name_len);
  name_copy[name_len] = '\0';

  char value_copy[PROC_ENV_VALUE_MAX];
  size_t value_len = 0;
  if(value != NULL) {
    value_len = strnlen(value, PROC_ENV_VALUE_MAX);
    if(value_len >= PROC_ENV_VALUE_MAX) {
      frame->rax = (uint64_t)(-E2BIG);
      return frame->rax;
    }
    memcpy(value_copy, value, value_len);
    value_copy[value_len] = '\0';
  } else {
    value_copy[0] = '\0';
  }

  int rc = proc_env_set(current, name_copy, value_copy, overwrite != 0);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_unsetenv_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* name = (const char*)frame->rdi;
  if(name == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  size_t name_len = strnlen(name, PROC_ENV_NAME_MAX);
  if(name_len == 0 || name_len >= PROC_ENV_NAME_MAX) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }
  for(size_t i = 0; i < name_len; i++) {
    if(name[i] == '=') {
      frame->rax = (uint64_t)(-EINVAL);
      return frame->rax;
    }
  }

  char name_copy[PROC_ENV_NAME_MAX];
  memcpy(name_copy, name, name_len);
  name_copy[name_len] = '\0';

  int rc = proc_env_unset(current, name_copy);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}
