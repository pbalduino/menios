#include <errno.h>

#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>

static inline void kmutex_wait_for_wakeup(void) {
#if defined(__x86_64__)
  asm volatile("hlt");
#else
  spinlock_cpu_relax();
#endif
}

struct kmutex_wait_node {
  kmutex_wait_node_t* next;
  proc_info_p         proc;
};

static void kmutex_enqueue_waiter(kmutex_t* mutex, kmutex_wait_node_t* node) {
  node->next = NULL;
  if(mutex->waiters_tail) {
    mutex->waiters_tail->next = node;
  } else {
    mutex->waiters_head = node;
  }
  mutex->waiters_tail = node;
}

static proc_info_p kmutex_dequeue_waiter(kmutex_t* mutex) {
  kmutex_wait_node_t* node = mutex->waiters_head;
  if(node) {
    mutex->waiters_head = node->next;
    if(mutex->waiters_head == NULL) {
      mutex->waiters_tail = NULL;
    }
    proc_info_p proc = node->proc;
    kfree(node);
    return proc;
  }
  return NULL;
}

int kmutex_lock(kmutex_t* mutex) {
  if(mutex == NULL) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  kmutex_wait_node_t* pending_node = NULL;

  for(;;) {
    spinlock_lock(&mutex->lock);

    if(mutex->owner == NULL) {
      mutex->owner = current;
      if(current) {
        current->errno = 0;
      }
      spinlock_unlock(&mutex->lock);
      if(pending_node) {
        kfree(pending_node);
        pending_node = NULL;
      }
      return 0;
    }

    if(mutex->owner == current) {
      spinlock_unlock(&mutex->lock);
      if(current) {
        current->errno = EDEADLK;
      }
      serial_error("kmutex_lock: recursive lock detected\n");
      if(pending_node) {
        kfree(pending_node);
      }
      return -EDEADLK;
    }

    if(current == NULL) {
      spinlock_unlock(&mutex->lock);
      if(pending_node) {
        kfree(pending_node);
        pending_node = NULL;
      }
      return -EINVAL;
    }

    if(pending_node == NULL) {
      pending_node = kmalloc(sizeof(*pending_node));
      if(pending_node == NULL) {
        spinlock_unlock(&mutex->lock);
        if(current) {
          current->errno = ENOMEM;
        }
        return -ENOMEM;
      }
    }

    pending_node->proc = current;
    current->state = PROC_STATE_WAITING;
    kmutex_enqueue_waiter(mutex, pending_node);
    pending_node = NULL;
    spinlock_unlock(&mutex->lock);

    proc_request_yield();
    enable_interrupts();
    while(current->state == PROC_STATE_WAITING) {
      kmutex_wait_for_wakeup();
    }
  }
}

bool kmutex_trylock(kmutex_t* mutex) {
  if(mutex == NULL) {
    if(current) {
      current->errno = EINVAL;
    }
    return false;
  }

  bool acquired = false;
  spinlock_lock(&mutex->lock);
  if(mutex->owner == NULL) {
    mutex->owner = current;
    acquired = true;
    if(current) {
      current->errno = 0;
    }
  }
  spinlock_unlock(&mutex->lock);
  return acquired;
}

int kmutex_unlock(kmutex_t* mutex) {
  if(mutex == NULL) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  spinlock_lock(&mutex->lock);

  if(mutex->owner != current) {
    spinlock_unlock(&mutex->lock);
    if(current) {
      current->errno = EPERM;
    }
    serial_error("kmutex_unlock: current does not own mutex\n");
    return -EPERM;
  }

  mutex->owner = NULL;
  proc_info_p next = kmutex_dequeue_waiter(mutex);
  spinlock_unlock(&mutex->lock);

  if(next != NULL) {
    next->state = PROC_STATE_READY;
    proc_mark_ready(next);
  }
  if(current) {
    current->errno = 0;
  }
  return 0;
}
