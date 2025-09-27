#ifndef MENIOS_INCLUDE_KERNEL_MUTEX_H
#define MENIOS_INCLUDE_KERNEL_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/spinlock.h>
#include <types.h>

typedef struct kmutex_t {
  spinlock_t lock;
  uint32_t   owner_pid;
} kmutex_t;

static inline void kmutex_init(kmutex_t* mutex) {
  spinlock_init(&mutex->lock);
  mutex->owner_pid = 0;
}

int kmutex_lock(kmutex_t* mutex);
bool kmutex_trylock(kmutex_t* mutex);
int kmutex_unlock(kmutex_t* mutex);

#ifdef __cplusplus
}
#endif

#endif
