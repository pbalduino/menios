#ifndef MENIOS_INCLUDE_SIGNAL_H
#define MENIOS_INCLUDE_SIGNAL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int sigset_t;

typedef union sigval {
  int   sival_int;
  void* sival_ptr;
} sigval_t;

typedef struct siginfo {
  int      si_signo;
  int      si_errno;
  int      si_code;
  sigval_t si_value;
  void*    si_addr;
  pid_t    si_pid;
  uid_t    si_uid;
} siginfo_t;

typedef void (*sighandler_t)(int);
typedef void (*sigaction_handler_t)(int, siginfo_t*, void*);

typedef void (*sigrestorer_t)(void);

typedef struct sigaction {
  union {
    sighandler_t        sa_handler;
    sigaction_handler_t sa_sigaction;
  };
  sigset_t      sa_mask;
  unsigned int  sa_flags;
  sigrestorer_t sa_restorer;
} sigaction_t;

#define SIGINT   2
#define SIGKILL  9
#define SIGALRM 14
#define SIGSEGV 11
#define SIGTERM 15
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)(-1))

#define SA_NOCLDSTOP 0x00000001u
#define SA_NOCLDWAIT 0x00000002u
#define SA_SIGINFO   0x00000004u
#define SA_ONSTACK   0x08000000u
#define SA_RESTART   0x10000000u
#define SA_NODEFER   0x40000000u
#define SA_RESETHAND 0x80000000u

#define __MENIOS_SIGSET_WIDTH 32u
#define __MENIOS_SIGSET_ALL_MASK 0x7FFFFFFFu

#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2

int kill(pid_t pid, int signo);
int sigaction(int signo, const struct sigaction* act, struct sigaction* oldact);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
sighandler_t signal(int signo, sighandler_t handler);
int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int signo);
int sigdelset(sigset_t* set, int signo);
int sigismember(const sigset_t* set, int signo);
int sigpending(sigset_t* set);
int sigsuspend(const sigset_t* mask);
int sigwait(const sigset_t* set, int* sig);
int sigwaitinfo(const sigset_t* set, siginfo_t* info);
int sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout);

#ifdef __cplusplus
}
#endif

#endif
