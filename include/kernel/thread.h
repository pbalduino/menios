#ifndef MENIOS_INCLUDE_KERNEL_THREAD_H
#define MENIOS_INCLUDE_KERNEL_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#define THREAD_RUNNING    0
#define THREAD_TERMINATED 1

#include <stdbool.h>

#include <kernel/condvar.h>
#include <kernel/mutex.h>

struct proc_info_t;

typedef struct kthread_t {
  const char* name;
  int         (*entrypoint)(void*);
  void*       arguments;
  struct proc_info_t* proc;
  kmutex_t    lock;
  kcondvar_t  cond;
  bool        finished;
  bool        joined;
  int         exit_code;
} kthread_t;

typedef kthread_t* kthread_p;

// typedef void *(*entrypoint)(void *) kthread_handler_t;

int kthread_create(kthread_t* thread, const char* name, int (*entrypoint)(void *), void* arg);

void ksleep(uint64_t milliseconds);
void kexit(int code);
int kthread_join(kthread_t* thread);

static inline int ktread_join(kthread_t* thread) {
  return kthread_join(thread);
}

#ifdef __cplusplus
}
#endif

#endif //MENIOS_INCLUDE_KERNEL_THREAD_H
