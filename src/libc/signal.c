#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <signal.h>
#include <sys/errno.h>

int kill(pid_t pid, int signo) {
  long rc = __menios_syscall2(SYS_KILL, (long)pid, (long)signo);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

int sigaction(int signo, const struct sigaction* act, struct sigaction* oldact) {
  long rc = __menios_syscall3(SYS_SIGACTION, (long)signo, (long)act, (long)oldact);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  long rc = __menios_syscall3(SYS_SIGPROCMASK, (long)how, (long)set, (long)oldset);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

#endif
