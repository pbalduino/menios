#include <kernel/signal.h>
#include <kernel/proc.h>

#include <string.h>

void proc_signal_state_init(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  proc->signal_pending = 0;
  proc->signal_blocked = 0;
  for(int signo = 0; signo < SIG_MAX; signo++) {
    proc->signal_handlers[signo] = SIG_DFL;
  }
}

void proc_signal_state_copy(proc_info_p dst, proc_info_p src) {
  if(dst == NULL || src == NULL) {
    return;
  }

  dst->signal_pending = 0;
  dst->signal_blocked = src->signal_blocked;
  memcpy(dst->signal_handlers, src->signal_handlers, sizeof(dst->signal_handlers));
}

void proc_signal_set_blocked(proc_info_p proc, uint32_t mask) {
  if(proc == NULL) {
    return;
  }
  proc->signal_blocked = mask;
}

void proc_signal_enqueue(proc_info_p proc, int signo) {
  if(proc == NULL) {
    return;
  }
  uint32_t bit = sigbit(signo);
  if(bit == 0) {
    return;
  }
  proc->signal_pending |= bit;
}

bool proc_signal_pending(proc_info_p proc) {
  if(proc == NULL) {
    return false;
  }
  return proc->signal_pending != 0;
}

int proc_signal_dequeue(proc_info_p proc) {
  if(proc == NULL) {
    return -1;
  }

  for(int signo = 1; signo < SIG_MAX; signo++) {
    uint32_t bit = sigbit(signo);
    if((proc->signal_pending & bit) == 0) {
      continue;
    }
    if(proc->signal_blocked & bit) {
      continue;
    }
    proc->signal_pending &= ~bit;
    return signo;
  }

  return -1;
}
