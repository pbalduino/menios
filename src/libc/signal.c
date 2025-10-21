#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <menios/signal_frame.h>
#include <signal.h>
#include <sys/errno.h>
#include <stdbool.h>
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

static inline int sigset_validate_ptr(sigset_t* set) {
  if(set == NULL) {
    errno = EINVAL;
    return -1;
  }
  return 0;
}

static inline int sigset_validate_const_ptr(const sigset_t* set) {
  if(set == NULL) {
    errno = EINVAL;
    return -1;
  }
  return 0;
}

static inline bool sigset_valid_signo(int signo) {
  return signo > 0 && signo < (int)__MENIOS_SIGSET_WIDTH;
}

static inline sigset_t sigset_mask(int signo) {
  return (sigset_t)(1u << (unsigned)(signo - 1));
}

int sigemptyset(sigset_t* set) {
  if(sigset_validate_ptr(set) < 0) {
    return -1;
  }
  *set = 0;
  return 0;
}

int sigfillset(sigset_t* set) {
  if(sigset_validate_ptr(set) < 0) {
    return -1;
  }
  *set = (sigset_t)__MENIOS_SIGSET_ALL_MASK;
  return 0;
}

int sigaddset(sigset_t* set, int signo) {
  if(sigset_validate_ptr(set) < 0) {
    return -1;
  }
  if(!sigset_valid_signo(signo)) {
    errno = EINVAL;
    return -1;
  }
  *set |= sigset_mask(signo);
  return 0;
}

int sigdelset(sigset_t* set, int signo) {
  if(sigset_validate_ptr(set) < 0) {
    return -1;
  }
  if(!sigset_valid_signo(signo)) {
    errno = EINVAL;
    return -1;
  }
  *set &= (sigset_t)~sigset_mask(signo);
  return 0;
}

int sigismember(const sigset_t* set, int signo) {
  if(sigset_validate_const_ptr(set) < 0) {
    return -1;
  }
  if(!sigset_valid_signo(signo)) {
    errno = EINVAL;
    return -1;
  }
  return ((*set & sigset_mask(signo)) != 0) ? 1 : 0;
}

int sigpending(sigset_t* set) {
  if(sigset_validate_ptr(set) < 0) {
    return -1;
  }
  errno = ENOSYS;
  return -1;
}

int sigsuspend(const sigset_t* mask) {
  if(sigset_validate_const_ptr(mask) < 0) {
    return -1;
  }
  errno = ENOSYS;
  return -1;
}

int sigwait(const sigset_t* set, int* sig) {
  (void)set;
  (void)sig;
  errno = ENOSYS;
  return -1;
}

int sigwaitinfo(const sigset_t* set, siginfo_t* info) {
  (void)set;
  (void)info;
  errno = ENOSYS;
  return -1;
}

int sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout) {
  (void)set;
  (void)info;
  (void)timeout;
  errno = ENOSYS;
  return -1;
}

#endif
