#include <kernel/signal.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

#include <menios/signal_frame.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <stddef.h>

_Static_assert(sizeof(menios_signal_context_t) == sizeof(cpu_state_t),
               "signal context size mismatch");
_Static_assert(offsetof(menios_signal_context_t, rip) == offsetof(cpu_state_t, rip),
               "rip offset mismatch");
_Static_assert(offsetof(menios_signal_context_t, rsp) == offsetof(cpu_state_t, rsp),
               "rsp offset mismatch");
_Static_assert(offsetof(menios_signal_context_t, cs) == offsetof(cpu_state_t, cs),
               "cs offset mismatch");
_Static_assert(offsetof(menios_signal_context_t, ss) == offsetof(cpu_state_t, ss),
               "ss offset mismatch");

static inline uint32_t unblockable_mask(void) {
  return sigbit(SIGKILL) | sigbit(SIGSTOP);
}

static inline void proc_signal_fill_siginfo(siginfo_t* info,
                                            int signo,
                                            proc_info_p proc) {
  if(info == NULL) {
    return;
  }
  memset(info, 0, sizeof(*info));
  info->si_signo = signo;
  info->si_errno = 0;
  info->si_code = 0;
  info->si_value.sival_ptr = NULL;
  info->si_addr = NULL;
  info->si_pid = proc ? (pid_t)proc->pid : 0;
  info->si_uid = 0;
}

int proc_signal_take_pending(proc_info_p proc,
                             uint32_t mask,
                             siginfo_t* info) {
  if(proc == NULL) {
    return -EINVAL;
  }

  mask &= SIGNAL_ALLOWED_MASK;
  mask &= ~unblockable_mask();

  if(mask == 0) {
    return 0;
  }

  for(int signo = 1; signo < SIG_MAX; signo++) {
    uint32_t bit = sigbit(signo);
    if((mask & bit) == 0) {
      continue;
    }
    if((proc->signal_pending & bit) == 0) {
      continue;
    }
    proc->signal_pending &= ~bit;
    proc_signal_fill_siginfo(info, signo, proc);
    return signo;
  }

  return 0;
}

void proc_signal_state_initialize(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  proc->signal_pending = 0;
  proc->signal_blocked = 0;
  proc->stopped = false;
  proc->stop_status_pending = false;
  proc->stop_status = 0;
  proc->continue_status = 0;
  proc->continued_pending = false;
  proc->signal_wait_active = false;
  proc->signal_wait_consume = false;
  proc->signal_wait_capture_info = false;
  proc->signal_wait_result_ready = false;
  proc->signal_wait_mask = 0;
  memset(&proc->signal_wait_info, 0, sizeof(proc->signal_wait_info));
  proc->signal_wait_result = 0;
  proc->signal_sigsuspend_active = false;
  proc->signal_sigsuspend_oldmask = 0;
  for(int signo = 0; signo < SIG_MAX; signo++) {
    proc->signal_actions[signo].sa_handler = SIG_DFL;
    proc->signal_actions[signo].sa_mask = 0;
    proc->signal_actions[signo].sa_flags = 0;
    proc->signal_actions[signo].sa_restorer = NULL;
  }
}

void proc_signal_state_copy(proc_info_p dst, proc_info_p src) {
  if(dst == NULL || src == NULL) {
    return;
  }

  dst->signal_pending = 0;
  dst->signal_blocked = src->signal_blocked & ~unblockable_mask();
  memcpy(dst->signal_actions, src->signal_actions, sizeof(dst->signal_actions));
  dst->stopped = false;
  dst->stop_status_pending = false;
  dst->stop_status = 0;
  dst->continue_status = 0;
  dst->continued_pending = false;
  dst->signal_wait_active = false;
  dst->signal_wait_consume = false;
  dst->signal_wait_capture_info = false;
  dst->signal_wait_result_ready = false;
  dst->signal_wait_mask = 0;
  memset(&dst->signal_wait_info, 0, sizeof(dst->signal_wait_info));
  dst->signal_wait_result = 0;
  dst->signal_sigsuspend_active = false;
  dst->signal_sigsuspend_oldmask = 0;
}

void proc_signal_set_blocked(proc_info_p proc, uint32_t mask) {
  if(proc == NULL) {
    return;
  }
  proc->signal_blocked = (mask & SIGNAL_ALLOWED_MASK) & ~unblockable_mask();
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

bool proc_signal_has_unblocked(proc_info_p proc) {
  if(proc == NULL) {
    return false;
  }

  for(int signo = 1; signo < SIG_MAX; signo++) {
    uint32_t bit = sigbit(signo);
    if((proc->signal_pending & bit) == 0) {
      continue;
    }
    if((proc->signal_blocked & bit) != 0) {
      continue;
    }
    return true;
  }

  return false;
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
    if(signo == SIGKILL || signo == SIGSTOP) {
      return -EINVAL;
    }

    struct sigaction prepared = *new_action;
    prepared.sa_mask &= SIGNAL_ALLOWED_MASK;
    slot->sa_handler = prepared.sa_handler;
    slot->sa_mask = prepared.sa_mask;
    slot->sa_flags = prepared.sa_flags;
    slot->sa_restorer = prepared.sa_restorer;
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

  proc->signal_blocked &= ~unblockable_mask();

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

  if((bit & unblockable_mask()) != 0) {
    proc_signal_enqueue(proc, signo);
    if((proc->signal_blocked & bit) == 0) {
      if(proc->state == PROC_STATE_SLEEPING || proc->state == PROC_STATE_WAITING) {
        proc_mark_ready(proc);
      }
    }
    return 0;
  }

  if(proc->signal_wait_active &&
     (proc->signal_wait_mask & bit) != 0) {
    proc->signal_wait_active = false;
    proc->signal_wait_result_ready = true;
    proc->signal_wait_result = signo;
    if(proc->signal_wait_capture_info) {
      proc_signal_fill_siginfo(&proc->signal_wait_info, signo, proc);
    }
    if(!proc->signal_wait_consume) {
      proc_signal_enqueue(proc, signo);
    }
    if(proc->state == PROC_STATE_SLEEPING || proc->state == PROC_STATE_WAITING) {
      proc_mark_ready(proc);
    }
    return 0;
  }

  proc_signal_enqueue(proc, signo);
  if((proc->signal_blocked & bit) == 0) {
    if(proc->state == PROC_STATE_SLEEPING || proc->state == PROC_STATE_WAITING) {
      proc_mark_ready(proc);
    }
  }
  return 0;
}

proc_signal_delivery_t proc_signal_handle_pending(proc_info_p proc,
                                                  struct cpu_state_t* frame) {
  if(proc == NULL || frame == NULL) {
    return PROC_SIGNAL_DELIVERY_NONE;
  }

  if(!proc->user_mode || proc == &kernel_process_info) {
    return PROC_SIGNAL_DELIVERY_NONE;
  }

  if(!proc_signal_pending(proc)) {
    return PROC_SIGNAL_DELIVERY_NONE;
  }

  while(true) {
    int signo = proc_signal_dequeue(proc);
    if(signo <= 0) {
      return PROC_SIGNAL_DELIVERY_NONE;
    }

    struct sigaction action = proc->signal_actions[signo];
    if(action.sa_handler == SIG_IGN) {
      if(signo == SIGSTOP) {
        proc_mark_stopped(proc, signo);
        return PROC_SIGNAL_DELIVERY_STOPPED;
      }
      continue;
    }

    if(action.sa_handler == SIG_DFL || action.sa_handler == SIG_ERR) {
      if(signo == SIGSTOP || signo == SIGTSTP) {
        proc_mark_stopped(proc, signo);
        return PROC_SIGNAL_DELIVERY_STOPPED;
      }

      if(signo == SIGCONT) {
        proc_mark_continued(proc);
        return PROC_SIGNAL_DELIVERY_HANDLED;
      }

      proc_exit_signal(signo);
      return PROC_SIGNAL_DELIVERY_TERMINATED;
    }

    if(action.sa_restorer == NULL) {
      proc_exit_signal(signo);
      return PROC_SIGNAL_DELIVERY_TERMINATED;
    }

    bool use_siginfo = (action.sa_flags & SA_SIGINFO) != 0;

    menios_signal_frame_t sigframe;
    memcpy(&sigframe.context, frame, sizeof(sigframe.context));
    sigframe.context.rax = (uint64_t)(-EINTR);
    sigframe.signal_mask = proc->signal_blocked;
    sigframe.signo = signo;
    sigframe.reserved = 0;

    size_t frame_size = sizeof(sigframe);
    size_t siginfo_size = use_siginfo ? sizeof(siginfo_t) : 0;
    size_t payload_size = frame_size + siginfo_size;
    virt_addr_t payload_base = frame->rsp - payload_size;
    payload_base &= ~((virt_addr_t)0xF);

    virt_addr_t siginfo_addr = use_siginfo ? payload_base : 0;
    virt_addr_t frame_base = payload_base + siginfo_size;

    if(!proc_user_copy_out(proc, frame_base, &sigframe, sizeof(sigframe))) {
      proc_exit_signal(signo);
      return PROC_SIGNAL_DELIVERY_TERMINATED;
    }

    if(use_siginfo) {
      siginfo_t info;
      memset(&info, 0, sizeof(info));
      info.si_signo = signo;
      info.si_errno = 0;
      info.si_code = 0;
      info.si_value.sival_ptr = NULL;
      info.si_addr = NULL;
      info.si_pid = proc->pid;
      info.si_uid = 0;

      if(!proc_user_copy_out(proc, siginfo_addr, &info, sizeof(info))) {
        proc_exit_signal(signo);
        return PROC_SIGNAL_DELIVERY_TERMINATED;
      }
    }

    virt_addr_t restorer_slot = frame_base - sizeof(uint64_t);

    if(!proc_user_write64(proc, restorer_slot, (uint64_t)action.sa_restorer)) {
      proc_exit_signal(signo);
      return PROC_SIGNAL_DELIVERY_TERMINATED;
    }

    serial_printf("deliver signal %d: rip=%lx rsp=%lx cs=%x ss=%x rax=%lx size=%lu frame_base=%lx restorer_slot=%lx frame_size=%lu\n",
                  signo,
                  sigframe.context.rip,
                  sigframe.context.rsp,
                  (unsigned int)sigframe.context.cs,
                  (unsigned int)sigframe.context.ss,
                  sigframe.context.rax,
                  (unsigned long)sizeof(menios_signal_context_t),
                  (unsigned long)frame_base,
                  (unsigned long)restorer_slot,
                  (unsigned long)frame_size);

    frame->rsp = restorer_slot;
    frame->rip = use_siginfo
                   ? (uint64_t)action.sa_sigaction
                   : (uint64_t)action.sa_handler;
    frame->rdi = (uint64_t)signo;
    frame->rsi = use_siginfo ? (uint64_t)siginfo_addr : 0;
    frame->rdx = use_siginfo ? (uint64_t)frame_base : 0;
    frame->rcx = 0;
    frame->r8 = 0;
    frame->r9 = 0;
    frame->rax = (uint64_t)(-EINTR);
    current->err_no = EINTR;
    proc->syscall_trap_frame_valid = false;

    return PROC_SIGNAL_DELIVERY_HANDLED;
  }
}
