#include <kernel/signal.h>
#include <kernel/proc.h>

#include <errno.h>
#include <string.h>

#define SIGNAL_ALLOWED_MASK ((SIG_MAX >= 32) ? 0x7FFFFFFFu : ((1u << (SIG_MAX - 1)) - 1u))

void proc_signal_state_init(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  proc->signal_pending = 0;
  proc->signal_blocked = 0;
  for(int signo = 0; signo < SIG_MAX; signo++) {
    proc->signal_actions[signo].sa_handler = SIG_DFL;
    proc->signal_actions[signo].sa_mask = 0;
    proc->signal_actions[signo].sa_flags = 0;
  }
}

void proc_signal_state_copy(proc_info_p dst, proc_info_p src) {
  if(dst == NULL || src == NULL) {
    return;
  }

  dst->signal_pending = 0;
  dst->signal_blocked = src->signal_blocked;
  memcpy(dst->signal_actions, src->signal_actions, sizeof(dst->signal_actions));
}

void proc_signal_set_blocked(proc_info_p proc, uint32_t mask) {
  if(proc == NULL) {
    return;
  }
  proc->signal_blocked = mask & SIGNAL_ALLOWED_MASK;
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

int proc_signal_configure_action(proc_info_p proc,
                                 int signo,
                                 const struct sigaction* new_action,
                                 struct sigaction* old_action) {
  if(proc == NULL) {
    return -EINVAL;
  }

  uint32_t bit = sigbit(signo);
  if(bit == 0) {
    return -EINVAL;
  }

  struct sigaction* slot = &proc->signal_actions[signo];

  if(old_action != NULL) {
    *old_action = *slot;
  }

  if(new_action != NULL) {
    if(signo == SIGKILL) {
      return -EINVAL;
    }

    struct sigaction prepared = *new_action;
    prepared.sa_mask &= SIGNAL_ALLOWED_MASK;
    slot->sa_handler = prepared.sa_handler;
    slot->sa_mask = prepared.sa_mask;
    slot->sa_flags = prepared.sa_flags;
  }

  return 0;
}

int proc_signal_modify_mask(proc_info_p proc,
                            int how,
                            uint32_t mask,
                            uint32_t* old_mask) {
  if(proc == NULL) {
    return -EINVAL;
  }

  if(old_mask != NULL) {
    *old_mask = proc->signal_blocked;
  }

  mask &= SIGNAL_ALLOWED_MASK;

  switch(how) {
    case SIG_BLOCK:
      proc->signal_blocked |= mask;
      break;
    case SIG_UNBLOCK:
      proc->signal_blocked &= ~mask;
      break;
    case SIG_SETMASK:
      proc->signal_blocked = mask;
      break;
    default:
      return -EINVAL;
  }

  return 0;
}

int proc_signal_send(proc_info_p proc, int signo) {
  if(proc == NULL) {
    return -ESRCH;
  }

  uint32_t bit = sigbit(signo);
  if(bit == 0) {
    return -EINVAL;
  }

  proc_signal_enqueue(proc, signo);
  return 0;
}
