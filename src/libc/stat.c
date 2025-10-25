#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <utime.h>
#include <stddef.h>

int mkdir(const char* path, mode_t mode) {
  long rc = __menios_syscall2(SYS_MKDIR, (long)path, (long)mode);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  return 0;
}

int fstat(int fd, struct stat* buf) {
  long rc = __menios_syscall2(SYS_FSTAT, (long)fd, (long)buf);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  return 0;
}

int stat(const char* path, struct stat* buf) {
  long rc = __menios_syscall2(SYS_STAT, (long)path, (long)buf);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  return 0;
}

int lstat(const char* path, struct stat* buf) {
  long rc = __menios_syscall2(SYS_LSTAT, (long)path, (long)buf);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  return 0;
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

int fchmod(int fd, mode_t mode) {
  (void)fd;
  (void)mode;
  errno = ENOSYS;
  return -1;
}

int utime(const char* filename, const struct utimbuf* times) {
  (void)filename;
  (void)times;
  errno = ENOSYS;
  return -1;
}
#endif
