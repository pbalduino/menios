#include <kernel/input.h>
#include <kernel/spinlock.h>
#include <string.h>

#define KEY_EVENT_QUEUE_SIZE 128

static menios_key_event_t event_queue[KEY_EVENT_QUEUE_SIZE];
static size_t event_head = 0;
static size_t event_tail = 0;
static spinlock_t event_lock = { .state = { .value = 0 } };

void keyboard_event_push(const menios_key_event_t* event) {
  if(event == NULL) {
    return;
  }

  spinlock_lock(&event_lock);
  size_t next_head = (event_head + 1) % KEY_EVENT_QUEUE_SIZE;
  if(next_head == event_tail) {
    event_tail = (event_tail + 1) % KEY_EVENT_QUEUE_SIZE;
  }
  memcpy(&event_queue[event_head], event, sizeof(menios_key_event_t));
  event_head = next_head;
  spinlock_unlock(&event_lock);
}

bool keyboard_event_try_pop(menios_key_event_t* event) {
  if(event == NULL) {
    return false;
  }

  bool available = false;
  spinlock_lock(&event_lock);
  if(event_head != event_tail) {
    memcpy(event, &event_queue[event_tail], sizeof(menios_key_event_t));
    event_tail = (event_tail + 1) % KEY_EVENT_QUEUE_SIZE;
    available = true;
  }
  spinlock_unlock(&event_lock);
  return available;
}
