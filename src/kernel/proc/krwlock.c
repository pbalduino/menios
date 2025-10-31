#include <kernel/rwlock.h>

void krwlock_initialize(krwlock_t* rwlock) {
  if(rwlock == NULL) {
    return;
  }

  kmutex_init(&rwlock->lock);
  kcondvar_init(&rwlock->readers_cond);
  kcondvar_init(&rwlock->writers_cond);
  rwlock->readers = 0;
  rwlock->writers_waiting = 0;
  rwlock->writer_active = false;
}

void krwlock_destroy(krwlock_t* rwlock) {
  (void)rwlock;
}

void krwlock_rdlock(krwlock_t* rwlock) {
  if(rwlock == NULL) {
    return;
  }

  kmutex_lock(&rwlock->lock);
  while(rwlock->writer_active || rwlock->writers_waiting > 0) {
    kcondvar_wait(&rwlock->readers_cond, &rwlock->lock);
  }
  rwlock->readers++;
  kmutex_unlock(&rwlock->lock);
}

bool krwlock_tryrdlock(krwlock_t* rwlock) {
  if(rwlock == NULL) {
    return false;
  }

  bool acquired = false;
  kmutex_lock(&rwlock->lock);
  if(!rwlock->writer_active && rwlock->writers_waiting == 0) {
    rwlock->readers++;
    acquired = true;
  }
  kmutex_unlock(&rwlock->lock);
  return acquired;
}

void krwlock_rdunlock(krwlock_t* rwlock) {
  if(rwlock == NULL) {
    return;
  }

  kmutex_lock(&rwlock->lock);
  if(rwlock->readers > 0) {
    rwlock->readers--;
  }

  if(rwlock->readers == 0 && rwlock->writers_waiting > 0) {
    kcondvar_signal(&rwlock->writers_cond);
  }
  kmutex_unlock(&rwlock->lock);
}

void krwlock_wrlock(krwlock_t* rwlock) {
  if(rwlock == NULL) {
    return;
  }

  kmutex_lock(&rwlock->lock);
  rwlock->writers_waiting++;
  while(rwlock->writer_active || rwlock->readers > 0) {
    kcondvar_wait(&rwlock->writers_cond, &rwlock->lock);
  }
  rwlock->writers_waiting--;
  rwlock->writer_active = true;
  kmutex_unlock(&rwlock->lock);
}

bool krwlock_trywrlock(krwlock_t* rwlock) {
  if(rwlock == NULL) {
    return false;
  }

  bool acquired = false;
  kmutex_lock(&rwlock->lock);
  if(!rwlock->writer_active && rwlock->readers == 0) {
    rwlock->writer_active = true;
    acquired = true;
  }
  kmutex_unlock(&rwlock->lock);
  return acquired;
}

void krwlock_wrunlock(krwlock_t* rwlock) {
  if(rwlock == NULL) {
    return;
  }

  kmutex_lock(&rwlock->lock);
  rwlock->writer_active = false;
  if(rwlock->writers_waiting > 0) {
    kcondvar_signal(&rwlock->writers_cond);
  } else {
    kcondvar_broadcast(&rwlock->readers_cond);
  }
  kmutex_unlock(&rwlock->lock);
}
