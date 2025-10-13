#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

int open(const char* path, int oflag, ...) {
  int mode = 0;
  if(oflag & O_CREAT) {
    va_list ap;
    va_start(ap, oflag);
    mode = va_arg(ap, int);
    va_end(ap);
  }

  long rc = __menios_syscall3(SYS_OPEN, (long)path, (long)oflag, (long)mode);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

int fcntl(int fd, int cmd, ...) {
  uint64_t arg = 0;
  if(cmd == F_SETFD) {
    va_list ap;
    va_start(ap, cmd);
    arg = (uint64_t)va_arg(ap, int);
    va_end(ap);
  }

  long rc = __menios_syscall3(SYS_FCNTL,
                              (long)fd,
                              (long)cmd,
                              (long)arg);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

#endif
