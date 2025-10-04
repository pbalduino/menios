#include <errno.h>
#include <stdbool.h>

#include <kernel/condvar.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/gdt.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>

static inline void kcondvar_wait_cycle(void) {
#if defined(__x86_64__)
  asm volatile("hlt");
#else
  spinlock_cpu_relax();
#endif
}

typedef struct kcondvar_wait_node {
  struct kcondvar_wait_node* next;
  proc_info_p                proc;
} kcondvar_wait_node_t;

static void kcondvar_enqueue(kcondvar_t* cv, kcondvar_wait_node_t* node) {
  node->next = NULL;
  if(cv->waiters_tail) {
    cv->waiters_tail->next = node;
  } else {
    cv->waiters_head = node;
  }
  cv->waiters_tail = node;
}

static kcondvar_wait_node_t* kcondvar_dequeue(kcondvar_t* cv) {
  kcondvar_wait_node_t* node = cv->waiters_head;
  if(node != NULL) {
    cv->waiters_head = node->next;
    if(cv->waiters_head == NULL) {
      cv->waiters_tail = NULL;
    }
  }
  return node;
}

void kcondvar_wait(kcondvar_t* cv, kmutex_t* mutex) {
  if(cv == NULL || mutex == NULL) {
    if(current) {
      current->errno = EINVAL;
    }
    return;
  }

  kcondvar_wait_node_t* node = kmalloc(sizeof(*node));
  if(node == NULL) {
    if(current) {
      current->errno = ENOMEM;
    }
    return;
  }

  node->proc = current;

  spinlock_lock(&cv->lock);
  if(current != NULL) {
    current->state = PROC_STATE_WAITING;
    kcondvar_enqueue(cv, node);
  }
  spinlock_unlock(&cv->lock);

  kmutex_unlock(mutex);

  uint64_t saved_cs = 0;
  uint64_t saved_ss = 0;
  bool restore_segments = false;
  if(current != NULL && current->cpu_state != NULL) {
    saved_cs = current->cpu_state->cs;
    saved_ss = current->cpu_state->ss;
    current->cpu_state->cs = KERNEL_CODE_SEGMENT;
    current->cpu_state->ss = KERNEL_DATA_SEGMENT;
    restore_segments = true;
  }

  proc_request_yield();
  enable_interrupts();
  serial_printf("kcondvar_wait: pid=%u entering wait (state=%u)\n",
                current ? current->pid : 0,
                current ? current->state : 0);
  while(current->state == PROC_STATE_WAITING) {
    kcondvar_wait_cycle();
  }
  if(current != NULL) {
    current->state = PROC_STATE_RUNNING;
    if(restore_segments) {
      current->cpu_state->cs = saved_cs;
      current->cpu_state->ss = saved_ss;
    }
  }
  serial_printf("kcondvar_wait: pid=%u resumed\n", current ? current->pid : 0);
  while(kmutex_lock(mutex) != 0) {
    proc_request_yield();
    enable_interrupts();
    while(current->state == PROC_STATE_WAITING) {
      kcondvar_wait_cycle();
    }
  }
}

void kcondvar_signal(kcondvar_t* cv) {
  if(cv == NULL) {
    return;
  }

  spinlock_lock(&cv->lock);
  kcondvar_wait_node_t* node = kcondvar_dequeue(cv);
  spinlock_unlock(&cv->lock);

  if(node != NULL) {
    if(node->proc != NULL) {
      serial_printf("kcondvar_signal: waking pid=%u (old state=%u)\n",
                    node->proc->pid,
                    node->proc->state);
      if(node->proc == current) {
        node->proc->state = PROC_STATE_RUNNING;
      } else {
        proc_mark_ready(node->proc);
      }
    }
    kfree(node);
  }
}

void kcondvar_broadcast(kcondvar_t* cv) {
  if(cv == NULL) {
    return;
  }

  spinlock_lock(&cv->lock);
  kcondvar_wait_node_t* node = kcondvar_dequeue(cv);
  spinlock_unlock(&cv->lock);

  while(node != NULL) {
    if(node->proc != NULL) {
      node->proc->state = PROC_STATE_READY;
      proc_mark_ready(node->proc);
    }
    kfree(node);

    spinlock_lock(&cv->lock);
    node = kcondvar_dequeue(cv);
    spinlock_unlock(&cv->lock);
  }
}
