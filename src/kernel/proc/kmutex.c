#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/thread.h>
#include <stdio.h>

int test_and_set(volatile int* lock) {
  return __sync_lock_test_and_set(lock, 1);
}

int kmutex_lock(kmutex_t* mutex) {
  while(test_and_set(&mutex->lock)) {
  }
  mutex->pid = current->pid;
  return 0;
}

int kmutex_unlock(kmutex_t* mutex) {
  __sync_lock_release(&mutex->lock);
  return 0;
}