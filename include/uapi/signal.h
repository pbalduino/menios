#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NSIG 32

typedef uint32_t sigset_t;
typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIGBIT(sig) (1u << (sig))

#define SIG_UNBLOCKABLE_MASK (SIGBIT(SIGKILL) | SIGBIT(SIGSTOP))

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SA_RESTART   (1u << 0)
#define SA_NODEFER   (1u << 1)
#define SA_RESETHAND (1u << 2)
#define SA_NOCLDSTOP (1u << 3)

#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGTRAP  5
#define SIGABRT  6
#define SIGBUS   7
#define SIGFPE   8
#define SIGKILL  9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22

struct sigaction {
  sighandler_t sa_handler;
  sigset_t     sa_mask;
  uint32_t     sa_flags;
  void (*sa_restorer)(void);
};

static inline int sigemptyset(sigset_t* set) {
  if(set == NULL) {
    return -1;
  }
  *set = 0;
  return 0;
}

static inline int sigfillset(sigset_t* set) {
  if(set == NULL) {
    return -1;
  }
  *set = 0xFFFFFFFFu;
  return 0;
}

static inline int sigaddset(sigset_t* set, int sig) {
  if(set == NULL || sig <= 0 || sig >= NSIG) {
    return -1;
  }
  *set |= SIGBIT(sig);
  return 0;
}

static inline int sigdelset(sigset_t* set, int sig) {
  if(set == NULL || sig <= 0 || sig >= NSIG) {
    return -1;
  }
  *set &= ~SIGBIT(sig);
  return 0;
}

#ifndef MENIOS_KERNEL
sighandler_t signal(int sig, sighandler_t handler);
int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
int sigreturn(void* frame);
#endif

#ifdef __cplusplus
}
#endif
