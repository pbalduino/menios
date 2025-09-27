#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/thread.h>

int kmutex_lock(kmutex_t* mutex) {
  if(mutex == NULL) {
    serial_printf("kmutex_lock: mutex is NULL\n");
    return -1;
  }

  spinlock_lock(&mutex->lock);
  if(current != NULL) {
    mutex->owner_pid = current->pid;
  }
  return 0;
}

bool kmutex_trylock(kmutex_t* mutex) {
  if(mutex == NULL) {
    return false;
  }

  if(spinlock_trylock(&mutex->lock)) {
    if(current != NULL) {
      mutex->owner_pid = current->pid;
    }
    return true;
  }
  return false;
}

int kmutex_unlock(kmutex_t* mutex) {
  if(mutex == NULL) {
    serial_printf("kmutex_unlock: mutex is NULL\n");
    return -1;
  }

  mutex->owner_pid = 0;
  spinlock_unlock(&mutex->lock);
  return 0;
}
