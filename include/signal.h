#ifndef MENIOS_INCLUDE_SIGNAL_H
#define MENIOS_INCLUDE_SIGNAL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int sigset_t;

typedef void (*sighandler_t)(int);

typedef struct sigaction {
  sighandler_t sa_handler;
  sigset_t     sa_mask;
  unsigned int sa_flags;
} sigaction_t;

#define SIGINT   2
#define SIGTERM 15
#define SIGKILL  9
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)(-1))

#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2

int kill(pid_t pid, int signo);
int sigaction(int signo, const struct sigaction* act, struct sigaction* oldact);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);

#ifdef __cplusplus
}
#endif

#endif
