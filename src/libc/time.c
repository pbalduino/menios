#ifndef MENIOS_KERNEL
#include <errno.h>
#endif
#include <menios/syscall.h>
#include <sys/time.h>
#include <time.h>

#ifndef MENIOS_HOST_TEST
#include <menios/syscall_user.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
extern long syscall(long number, ...);
#endif

time_t time(time_t* tloc) {
#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall1(SYS_TIME, (long)tloc);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return (time_t)-1;
  }
  return (time_t)rc;
#else
  long rc = syscall(SYS_time, tloc);
  if(rc == -1) {
    return (time_t)-1;
  }
  return (time_t)rc;
#endif
}

int gettimeofday(struct timeval* tv, struct timezone* tz) {
#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall2(SYS_GETTIMEOFDAY, (long)tv, (long)tz);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return -1;
  }
  return 0;
#else
  long rc = syscall(SYS_gettimeofday, tv, tz);
  if(rc == -1) {
    return -1;
  }
  return 0;
#endif
}
