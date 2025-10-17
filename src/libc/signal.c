#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <signal.h>
#include <sys/errno.h>
#include <time.h>

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

sighandler_t signal(int signo, sighandler_t handler) {
  struct sigaction act = {
    .sa_handler = handler,
    .sa_mask = 0,
    .sa_flags = 0
  };

  struct sigaction old_act;
  if(sigaction(signo, &act, &old_act) < 0) {
    return SIG_ERR;
  }
  return old_act.sa_handler;
}

int pause(void) {
  for(;;) {
    struct timespec req = { .tv_sec = 86400, .tv_nsec = 0 };
    if(nanosleep(&req, NULL) == -1) {
      if(errno == EINTR) {
        errno = EINTR;
      }
      return -1;
    }
  }
}

#endif
