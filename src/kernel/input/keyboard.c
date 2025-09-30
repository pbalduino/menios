#include <kernel/input/keyboard.h>

#include <errno.h>
#include <stddef.h>

#include <kernel/condvar.h>
#include <kernel/file.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>

#define KEYBOARD_EVENT_CAPACITY 128

typedef struct keyboard_queue_t {
  spinlock_t lock;
  kmutex_t   wait_lock;
  kcondvar_t waiters;
  keyboard_event_t events[KEYBOARD_EVENT_CAPACITY];
  size_t head;
  size_t tail;
  bool initialized;
} keyboard_queue_t;

static keyboard_queue_t keyboard_queue;

static const file_ops_t keyboard_file_ops;

static inline bool queue_empty(const keyboard_queue_t* queue) {
  return queue->head == queue->tail;
}

static bool queue_pop(keyboard_queue_t* queue, keyboard_event_t* out) {
  bool has_event = false;
  spinlock_lock(&queue->lock);
  if(!queue_empty(queue)) {
    *out = queue->events[queue->tail];
    queue->tail = (queue->tail + 1) % KEYBOARD_EVENT_CAPACITY;
    has_event = true;
    serial_printf("keyboard_queue: pop scancode=0x%03x ascii=0x%02x pressed=%u head=%lu tail=%lu\n",
                  (unsigned)out->scancode,
                  (unsigned)out->ascii,
                  (unsigned)out->pressed,
                  (unsigned long)queue->head,
                  (unsigned long)queue->tail);
  } else {
    serial_printf("keyboard_queue: pop attempted on empty queue\n");
  }
  spinlock_unlock(&queue->lock);
  return has_event;
}

static bool queue_push(keyboard_queue_t* queue, const keyboard_event_t* event) {
  bool was_empty;
  spinlock_lock(&queue->lock);
  was_empty = queue_empty(queue);
  size_t next = (queue->head + 1) % KEYBOARD_EVENT_CAPACITY;
  if(next == queue->tail) {
    serial_printf("keyboard_queue: overflow, dropping tail scancode=0x%03x\n",
                  (unsigned)queue->events[queue->tail].scancode);
    queue->tail = (queue->tail + 1) % KEYBOARD_EVENT_CAPACITY;
  }
  queue->events[queue->head] = *event;
  queue->head = next;
  serial_printf("keyboard_queue: push scancode=0x%03x ascii=0x%02x pressed=%u new_head=%lu tail=%lu\n",
                (unsigned)event->scancode,
                (unsigned)event->ascii,
                (unsigned)event->pressed,
                (unsigned long)queue->head,
                (unsigned long)queue->tail);
  spinlock_unlock(&queue->lock);
  return was_empty;
}

void keyboard_device_init(void) {
  if(keyboard_queue.initialized) {
    serial_printf("keyboard_device_init: already initialized\n");
    return;
  }

  serial_printf("keyboard_device_init: initializing queue\n");
  spinlock_init(&keyboard_queue.lock);
  kmutex_init(&keyboard_queue.wait_lock);
  kcondvar_init(&keyboard_queue.waiters);
  keyboard_queue.head = 0;
  keyboard_queue.tail = 0;
  keyboard_queue.initialized = true;
  serial_printf("keyboard_device_init: done\n");
}

void keyboard_device_reset(void) {
  if(!keyboard_queue.initialized) {
    keyboard_device_init();
  }

  serial_printf("keyboard_device_reset: resetting queue\n");
  kmutex_lock(&keyboard_queue.wait_lock);
  spinlock_lock(&keyboard_queue.lock);
  keyboard_queue.head = 0;
  keyboard_queue.tail = 0;
  spinlock_unlock(&keyboard_queue.lock);
  kmutex_unlock(&keyboard_queue.wait_lock);
}

void keyboard_device_enqueue(const keyboard_event_t* event) {
  if(event == NULL) {
    serial_printf("keyboard_device_enqueue: NULL event\n");
    return;
  }

  if(!keyboard_queue.initialized) {
    keyboard_device_init();
  }

  bool was_empty = queue_push(&keyboard_queue, event);
  serial_printf("keyboard_device_enqueue: event scancode=0x%03x ascii=0x%02x pressed=%u was_empty=%s\n",
                (unsigned)event->scancode,
                (unsigned)event->ascii,
                (unsigned)event->pressed,
                was_empty ? "yes" : "no");
  kcondvar_signal(&keyboard_queue.waiters);
}

static int64_t keyboard_device_read(file_t* file, void* buffer, size_t length) {
  (void)file;

  if(buffer == NULL || length < sizeof(keyboard_event_t)) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  if(!keyboard_queue.initialized) {
    keyboard_device_init();
  }

  size_t max_events = length / sizeof(keyboard_event_t);
  if(max_events == 0) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  keyboard_event_t* out = (keyboard_event_t*)buffer;
  size_t consumed = 0;

  while(consumed < max_events) {
    keyboard_event_t evt;
    if(queue_pop(&keyboard_queue, &evt)) {
      out[consumed++] = evt;
      serial_printf("keyboard_device_read: immediate pop scancode=0x%03x ascii=0x%02x pressed=%u consumed=%lu/%lu\n",
                    (unsigned)evt.scancode,
                    (unsigned)evt.ascii,
                    (unsigned)evt.pressed,
                    (unsigned long)consumed,
                    (unsigned long)max_events);
      continue;
    }

    if(consumed > 0) {
      serial_printf("keyboard_device_read: partial read satisfied consumed=%lu\n", (unsigned long)consumed);
      break;
    }

    kmutex_lock(&keyboard_queue.wait_lock);
    for(;;) {
      if(queue_pop(&keyboard_queue, &evt)) {
        kmutex_unlock(&keyboard_queue.wait_lock);
        out[consumed++] = evt;
        serial_printf("keyboard_device_read: waited event scancode=0x%03x ascii=0x%02x pressed=%u\n",
                      (unsigned)evt.scancode,
                      (unsigned)evt.ascii,
                      (unsigned)evt.pressed);
        break;
      }
      serial_printf("keyboard_device_read: waiting on condvar\n");
      kcondvar_wait(&keyboard_queue.waiters, &keyboard_queue.wait_lock);
    }
  }

  serial_printf("keyboard_device_read: returning bytes=%lu\n",
                (unsigned long)(consumed * sizeof(keyboard_event_t)));

  return (int64_t)(consumed * sizeof(keyboard_event_t));
}

static const file_ops_t keyboard_file_ops = {
  .read = keyboard_device_read,
  .write = NULL,
  .close = NULL,
  .seek = NULL,
};

file_t* keyboard_device_open(void) {
  if(!keyboard_queue.initialized) {
    keyboard_device_init();
  }

  file_t* file = file_create(&keyboard_file_ops, NULL, FILE_MODE_READ);
  serial_printf("keyboard_device_open: file=%p\n", (void*)file);
  if(file == NULL && current != NULL && current->errno == 0) {
    current->errno = ENOMEM;
  }
  return file;
}
