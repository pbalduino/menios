#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/thread.h>

#include <errno.h>
#include <string.h>

static void kthread_mark_finished(kthread_t* thread, int exit_code) {
  kmutex_lock(&thread->lock);
  thread->exit_code = exit_code;
  thread->finished = true;
  kcondvar_broadcast(&thread->cond);
  kmutex_unlock(&thread->lock);
}

void kthread_execute(void* arg) {
  kthread_t* thread = (kthread_t*)arg;
  int res = -EINVAL;

  if(thread->entrypoint != NULL) {
    res = thread->entrypoint(thread->arguments);
  }

  kthread_mark_finished(thread, res);
  kexit(res);
}

int kthread_create(kthread_t* thread, const char* name, int (*entrypoint)(void *), void* arg) {
  if(thread == NULL || name == NULL || entrypoint == NULL) {
    return -EINVAL;
  }

  if(thread->proc != NULL) {
    return -EBUSY;
  }

  kmutex_init(&thread->lock);
  kcondvar_init(&thread->cond);
  thread->name = name;
  thread->entrypoint = entrypoint;
  thread->arguments = arg;
  thread->proc = NULL;
  thread->finished = false;
  thread->joined = false;
  thread->exit_code = 0;

  proc_info_p proc = kmalloc(sizeof(proc_info_t));
  if(proc == NULL) {
    return -ENOMEM;
  }

  memzero(proc, sizeof(proc_info_t));
  thread->proc = proc;

  proc_create(proc, thread->name, kthread_execute, thread);
  proc_execute(proc);
  return 0;
}

void ksleep(uint64_t milliseconds) {
  uint64_t usec = milliseconds * 1000ull;
  proc_request_sleep(usec);
  enable_interrupts();
  while(current->state == PROC_STATE_SLEEPING) {
    asm volatile("hlt");
  }
}

void kexit(int code) {
  proc_exit(code);
  enable_interrupts();
  for(;;) {
    asm volatile("hlt");
  }
}

int kthread_join(kthread_t* thread) {
  if(thread == NULL) {
    return -EINVAL;
  }

  kmutex_lock(&thread->lock);

  if(thread->proc == NULL) {
    kmutex_unlock(&thread->lock);
    return -EINVAL;
  }

  if(thread->joined) {
    kmutex_unlock(&thread->lock);
    return -EINVAL;
  }

  while(!thread->finished) {
    kcondvar_wait(&thread->cond, &thread->lock);
  }

  thread->joined = true;
  thread->proc = NULL;
  int exit_code = thread->exit_code;
  kmutex_unlock(&thread->lock);
  return exit_code;
}
