#ifndef MENIOS_INCLUDE_KERNEL_MUTEX_H
#define MENIOS_INCLUDE_KERNEL_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

typedef struct kthread_mutex_t {
  int      lock;
  uint32_t pid;
} kmutex_t;

int kmutex_lock(kmutex_t* mutex);
int kmutex_unlock(kmutex_t* mutex);

#ifdef __cplusplus
}
#endif

#endif