#include <kernel/workqueue.h>

#include <kernel/condvar.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/semaphore.h>
#include <kernel/spinlock.h>
#include <kernel/thread.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct workqueue_item {
  workqueue_class_t cls;
  workqueue_handler_t handler;
  void* ctx;
  struct workqueue_item* next;
} workqueue_item_t;

typedef struct {
  bool initialized;
  bool started;
  spinlock_t lock;
  ksem_t sem;
  workqueue_item_t* head;
  workqueue_item_t* tail;
  size_t pending_count;
  bool worker_active;
  kmutex_t wait_lock;
  kcondvar_t wait_cond;
  kthread_t worker_thread;
} workqueue_state_t;

static workqueue_state_t workqueue_state;

static void workqueue_enqueue_locked(workqueue_item_t* item) {
  if(workqueue_state.tail == NULL) {
    workqueue_state.head = item;
    workqueue_state.tail = item;
  } else {
    workqueue_state.tail->next = item;
    workqueue_state.tail = item;
  }
  workqueue_state.pending_count++;
}

static workqueue_item_t* workqueue_pop_locked(void) {
  workqueue_item_t* item = workqueue_state.head;
  if(item != NULL) {
    workqueue_state.head = item->next;
    if(workqueue_state.head == NULL) {
      workqueue_state.tail = NULL;
    }
  }
  if(item != NULL && workqueue_state.pending_count > 0) {
    workqueue_state.pending_count--;
  }
  return item;
}

static void workqueue_notify_idle(void) {
  kmutex_lock(&workqueue_state.wait_lock);
  kcondvar_broadcast(&workqueue_state.wait_cond);
  kmutex_unlock(&workqueue_state.wait_lock);
}

static void workqueue_process_pending_sync(void) {
  for(;;) {
    workqueue_item_t* item = NULL;

    spinlock_lock(&workqueue_state.lock);
    item = workqueue_pop_locked();
    spinlock_unlock(&workqueue_state.lock);

    if(item == NULL) {
      break;
    }

    if(item->handler != NULL) {
      item->handler(item->ctx);
    }
    kfree(item);
  }

  spinlock_lock(&workqueue_state.lock);
  workqueue_state.worker_active = false;
  bool idle = (workqueue_state.pending_count == 0);
  spinlock_unlock(&workqueue_state.lock);

  if(idle) {
    workqueue_notify_idle();
  }
}

static int workqueue_thread_main(void* arg) {
  (void)arg;

  for(;;) {
    ksem_wait(&workqueue_state.sem);

    workqueue_item_t* item = NULL;

    spinlock_lock(&workqueue_state.lock);
    item = workqueue_pop_locked();
    workqueue_state.worker_active = (item != NULL);
    spinlock_unlock(&workqueue_state.lock);

    if(item == NULL) {
      continue;
    }

    if(item->handler != NULL) {
      item->handler(item->ctx);
    }

    kfree(item);

    spinlock_lock(&workqueue_state.lock);
    workqueue_state.worker_active = false;
    bool idle = (workqueue_state.pending_count == 0);
    spinlock_unlock(&workqueue_state.lock);

    if(idle) {
      workqueue_notify_idle();
    }
  }

  return 0;
}

void workqueue_initialize(void) {
  if(workqueue_state.initialized) {
    return;
  }

  spinlock_init(&workqueue_state.lock);
  ksem_initialize(&workqueue_state.sem, 0);
  kmutex_init(&workqueue_state.wait_lock);
  kcondvar_init(&workqueue_state.wait_cond);

  workqueue_state.head = NULL;
  workqueue_state.tail = NULL;
  workqueue_state.pending_count = 0;
  workqueue_state.worker_active = false;
  workqueue_state.started = false;
  workqueue_state.initialized = true;
}

void workqueue_start(void) {
  if(!workqueue_state.initialized || workqueue_state.started) {
    return;
  }

  if(kthread_create(&workqueue_state.worker_thread,
                    "workqueue",
                    workqueue_thread_main,
                    NULL) == 0) {
    workqueue_state.started = true;

    size_t pending = 0;
    spinlock_lock(&workqueue_state.lock);
    pending = workqueue_state.pending_count;
    spinlock_unlock(&workqueue_state.lock);

    for(size_t i = 0; i < pending; ++i) {
      ksem_post(&workqueue_state.sem);
    }
  }
}

int workqueue_submit(workqueue_class_t cls,
                     workqueue_handler_t handler,
                     void* ctx) {
  if(!workqueue_state.initialized || handler == NULL) {
    return -EINVAL;
  }

  workqueue_item_t* item = kmalloc(sizeof(*item));
  if(item == NULL) {
    return -ENOMEM;
  }

  item->cls = cls;
  item->handler = handler;
  item->ctx = ctx;
  item->next = NULL;

  spinlock_lock(&workqueue_state.lock);
  workqueue_enqueue_locked(item);
  bool should_signal = workqueue_state.started;
  spinlock_unlock(&workqueue_state.lock);

  if(should_signal) {
    ksem_post(&workqueue_state.sem);
  }

  return 0;
}

void workqueue_wait_idle(void) {
  if(!workqueue_state.initialized) {
    return;
  }

  if(!workqueue_state.started) {
    workqueue_process_pending_sync();
    return;
  }

  kmutex_lock(&workqueue_state.wait_lock);
  for(;;) {
    bool idle = false;

    spinlock_lock(&workqueue_state.lock);
    idle = (workqueue_state.pending_count == 0) && !workqueue_state.worker_active;
    spinlock_unlock(&workqueue_state.lock);

    if(idle) {
      break;
    }

    kcondvar_wait(&workqueue_state.wait_cond, &workqueue_state.wait_lock);
  }
  kmutex_unlock(&workqueue_state.wait_lock);
}
