#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <menios/signal_frame.h>
#include <signal.h>
#include <sys/errno.h>
#include <time.h>
#ifdef MENIOS_HOST_TEST
#include <stdlib.h>
#endif

#ifndef MENIOS_HOST_TEST
void __attribute__((naked, noreturn)) __menios_sigrestorer(void) {
  __asm__ volatile(
    "mov %%rsp, %%rdi\n\t"
    "mov %0, %%rax\n\t"
    "syscall\n\t"
    "ud2\n"
    :
    : "i"(SYS_SIGRETURN)
    : "rax", "rdi"
  );
}
#else
void __attribute__((noreturn)) __menios_sigrestorer(void) {
  abort();
}
#endif

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
  struct sigaction prepared;
  const struct sigaction* act_ptr = act;

  if(act != NULL && act->sa_restorer == NULL) {
    prepared = *act;
    prepared.sa_restorer = __menios_sigrestorer;
    act_ptr = &prepared;
  }

  long rc = __menios_syscall3(SYS_SIGACTION, (long)signo, (long)act_ptr, (long)oldact);
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
    .sa_flags = 0,
    .sa_restorer = __menios_sigrestorer
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
