#ifndef MENIOS_INCLUDE_KERNEL_CONDVAR_H
#define MENIOS_INCLUDE_KERNEL_CONDVAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/mutex.h>
#include <kernel/spinlock.h>

struct proc_info_t;
struct kcondvar_wait_node;

typedef struct kcondvar_t {
  spinlock_t             lock;
  struct kcondvar_wait_node* waiters_head;
  struct kcondvar_wait_node* waiters_tail;
} kcondvar_t;

static inline void kcondvar_init(kcondvar_t* cv) {
  spinlock_init(&cv->lock);
  cv->waiters_head = NULL;
  cv->waiters_tail = NULL;
}

void kcondvar_wait(kcondvar_t* cv, kmutex_t* mutex);
void kcondvar_signal(kcondvar_t* cv);
void kcondvar_broadcast(kcondvar_t* cv);

#ifdef __cplusplus
}
#endif

#endif
