#ifndef MENIOS_INCLUDE_KERNEL_SIGNAL_H
#define MENIOS_INCLUDE_KERNEL_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

struct proc_info_t;
struct cpu_state_t;

typedef struct proc_info_t* proc_info_p;

#define SIG_MAX 32

static inline uint32_t sigbit(int signo) {
  if(signo <= 0 || signo >= SIG_MAX) {
    return 0;
  }
  return (uint32_t)(1u << (signo - 1));
}

void proc_signal_state_init(proc_info_p proc);
void proc_signal_state_copy(proc_info_p dst, proc_info_p src);
void proc_signal_set_blocked(proc_info_p proc, uint32_t mask);
void proc_signal_enqueue(proc_info_p proc, int signo);
bool proc_signal_pending(proc_info_p proc);
int  proc_signal_dequeue(proc_info_p proc);
typedef enum {
  PROC_SIGNAL_DELIVERY_NONE = 0,
  PROC_SIGNAL_DELIVERY_HANDLED,
  PROC_SIGNAL_DELIVERY_STOPPED,
  PROC_SIGNAL_DELIVERY_TERMINATED
} proc_signal_delivery_t;

proc_signal_delivery_t proc_signal_handle_pending(proc_info_p proc,
                                                  struct cpu_state_t* frame);
int  proc_signal_configure_action(proc_info_p proc,
                                  int signo,
                                  const struct sigaction* new_action,
                                  struct sigaction* old_action);
int  proc_signal_modify_mask(proc_info_p proc,
                             int how,
                             uint32_t mask,
                             uint32_t* old_mask);
int  proc_signal_send(proc_info_p proc, int signo);

#ifdef __cplusplus
}
#endif

#endif
