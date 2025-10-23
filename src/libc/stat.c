#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <stddef.h>
#include <string.h>

static void clear_stat_buffer(struct stat* buf) {
  if(buf != NULL) {
    memset(buf, 0, sizeof(*buf));
  }
}

int mkdir(const char* path, mode_t mode) {
  long rc = __menios_syscall2(SYS_MKDIR, (long)path, (long)mode);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}

int fstat(int fd, struct stat* buf) {
  (void)fd;
  clear_stat_buffer(buf);
  errno = ENOSYS;
  return -1;
}

int stat(const char* path, struct stat* buf) {
  (void)path;
  clear_stat_buffer(buf);
  errno = ENOSYS;
  return -1;
}

int lstat(const char* path, struct stat* buf) {
  (void)path;
  clear_stat_buffer(buf);
  errno = ENOSYS;
  return -1;
}

int chmod(const char* path, mode_t mode) {
  (void)path;
  (void)mode;
  errno = ENOSYS;
  return -1;
}

mode_t umask(mode_t mask) {
  static mode_t current_mask = 0022;
  mode_t previous = current_mask;
  current_mask = mask & 0777;
  return previous;
}
#endif
