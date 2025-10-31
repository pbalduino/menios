#ifndef MENIOS_INCLUDE_KERNEL_RWLOCK_H
#define MENIOS_INCLUDE_KERNEL_RWLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <kernel/condvar.h>
#include <kernel/mutex.h>

typedef struct krwlock_t {
  kmutex_t    lock;
  kcondvar_t  readers_cond;
  kcondvar_t  writers_cond;
  uint32_t    readers;
  uint32_t    writers_waiting;
  bool        writer_active;
} krwlock_t;

void krwlock_initialize(krwlock_t* rwlock);
void krwlock_destroy(krwlock_t* rwlock);

void krwlock_rdlock(krwlock_t* rwlock);
bool krwlock_tryrdlock(krwlock_t* rwlock);
void krwlock_rdunlock(krwlock_t* rwlock);

void krwlock_wrlock(krwlock_t* rwlock);
bool krwlock_trywrlock(krwlock_t* rwlock);
void krwlock_wrunlock(krwlock_t* rwlock);

#ifdef __cplusplus
}
#endif

#endif
