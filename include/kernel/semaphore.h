#ifndef MENIOS_INCLUDE_KERNEL_SEMAPHORE_H
#define MENIOS_INCLUDE_KERNEL_SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <kernel/condvar.h>
#include <kernel/mutex.h>

typedef struct ksem_t {
  kmutex_t lock;
  kcondvar_t cond;
  int64_t count;
} ksem_t;

typedef ksem_t* ksem_p;

void ksem_init(ksem_t* sem, int64_t value);
void ksem_destroy(ksem_t* sem);
bool ksem_trywait(ksem_t* sem);
void ksem_wait(ksem_t* sem);
void ksem_post(ksem_t* sem);

#ifdef __cplusplus
}
#endif

#endif
