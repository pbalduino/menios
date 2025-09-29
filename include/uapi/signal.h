#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NSIG 32

#define SIGINT   2
#define SIGKILL  9
#define SIGTERM 15
#define SIGSTOP 19

typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

struct sigaction {
  sighandler_t sa_handler;
  uint32_t     sa_mask;
  uint32_t     sa_flags;
};

#ifndef MENIOS_KERNEL
sighandler_t signal(int sig, sighandler_t handler);
int sigreturn(void* frame);
#endif

#ifdef __cplusplus
}
#endif
