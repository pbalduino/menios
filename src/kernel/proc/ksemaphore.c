#include <kernel/semaphore.h>

void ksem_init(ksem_t* sem, int64_t value) {
  if(sem == NULL) {
    return;
  }
  kmutex_init(&sem->lock);
  kcondvar_init(&sem->cond);
  sem->count = value;
}

void ksem_destroy(ksem_t* sem) {
  (void)sem;
}

bool ksem_trywait(ksem_t* sem) {
  if(sem == NULL) {
    return false;
  }

  bool acquired = false;
  kmutex_lock(&sem->lock);
  if(sem->count > 0) {
    sem->count--;
    acquired = true;
  }
  kmutex_unlock(&sem->lock);
  return acquired;
}

void ksem_wait(ksem_t* sem) {
  if(sem == NULL) {
    return;
  }

  kmutex_lock(&sem->lock);
  while(sem->count == 0) {
    kcondvar_wait(&sem->cond, &sem->lock);
  }
  sem->count--;
  kmutex_unlock(&sem->lock);
}

void ksem_post(ksem_t* sem) {
  if(sem == NULL) {
    return;
  }

  kmutex_lock(&sem->lock);
  sem->count++;
  kcondvar_signal(&sem->cond);
  kmutex_unlock(&sem->lock);
}
