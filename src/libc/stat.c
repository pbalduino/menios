#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <sys/errno.h>
#include <sys/stat.h>

int mkdir(const char* path, mode_t mode) {
  long rc = __menios_syscall2(SYS_MKDIR, (long)path, (long)mode);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}
#endif
