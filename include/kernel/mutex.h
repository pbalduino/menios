#ifndef MENIOS_INCLUDE_KERNEL_MUTEX_H
#define MENIOS_INCLUDE_KERNEL_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/spinlock.h>
#include <types.h>

struct proc_info_t;
typedef struct kmutex_wait_node kmutex_wait_node_t;

typedef struct kmutex_t {
  spinlock_t            lock;
  struct proc_info_t*   owner;
  kmutex_wait_node_t* waiters_head;
  kmutex_wait_node_t* waiters_tail;
} kmutex_t;

static inline void kmutex_init(kmutex_t* mutex) {
  spinlock_init(&mutex->lock);
  mutex->owner = NULL;
  mutex->waiters_head = NULL;
  mutex->waiters_tail = NULL;
}

int kmutex_lock(kmutex_t* mutex);
bool kmutex_trylock(kmutex_t* mutex);
int kmutex_unlock(kmutex_t* mutex);

#ifdef __cplusplus
}
#endif

#endif
