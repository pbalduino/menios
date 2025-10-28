#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <utime.h>
#include <stddef.h>

#ifdef MENIOS_HOST_TEST
#include <fcntl.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

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
#ifdef MENIOS_HOST_TEST
  if(path == NULL) {
    errno = EFAULT;
    return -1;
  }

  return fchmodat(AT_FDCWD, path, mode, 0);
#elif defined(SYS_CHMOD)
  long rc = __menios_syscall2(SYS_CHMOD, (long)path, (long)mode);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }
  return 0;
#else
  (void)path;
  (void)mode;
  errno = ENOSYS;
  return -1;
#endif
}

mode_t umask(mode_t mask) {
  static mode_t current_mask = 0022;
  mode_t previous = current_mask;
  current_mask = mask & 0777;
  return previous;
}

int fchmod(int fd, mode_t mode) {
#ifdef MENIOS_HOST_TEST
#ifdef AT_EMPTY_PATH
  int rc = fchmodat(fd, "", mode, AT_EMPTY_PATH);
  if(rc == 0 || errno != ENOTSUP) {
    return rc;
  }
#endif
  char fd_path[64];
  int written = snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
  if(written <= 0 || (size_t)written >= sizeof(fd_path)) {
    errno = EINVAL;
    return -1;
  }
  return fchmodat(AT_FDCWD, fd_path, mode, 0);
#elif defined(SYS_FCHMOD)
  long rc = __menios_syscall2(SYS_FCHMOD, (long)fd, (long)mode);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }
  return 0;
#else
  (void)fd;
  (void)mode;
  errno = ENOSYS;
  return -1;
#endif
}

int utime(const char* filename, const struct utimbuf* times) {
#ifdef MENIOS_HOST_TEST
  if(filename == NULL) {
    errno = EFAULT;
    return -1;
  }

#ifdef UTIME_NOW
  struct timespec ts[2];
  const struct timespec* ts_ptr = NULL;
  if(times != NULL) {
    ts[0].tv_sec = times->actime;
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = times->modtime;
    ts[1].tv_nsec = 0;
    ts_ptr = ts;
  }
  int rc = utimensat(AT_FDCWD, filename, ts_ptr, 0);
  if(rc == 0 || errno != ENOSYS) {
    return rc;
  }
#endif

  struct timeval tv[2];
  struct timeval* tv_ptr = NULL;
  if(times != NULL) {
    tv[0].tv_sec = times->actime;
    tv[0].tv_usec = 0;
    tv[1].tv_sec = times->modtime;
    tv[1].tv_usec = 0;
    tv_ptr = tv;
  }
  return utimes(filename, tv_ptr);
#elif defined(SYS_UTIME)
  long rc = __menios_syscall2(SYS_UTIME, (long)filename, (long)times);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }
  return 0;
#else
  (void)filename;
  (void)times;
  errno = ENOSYS;
  return -1;
#endif
}
#endif
