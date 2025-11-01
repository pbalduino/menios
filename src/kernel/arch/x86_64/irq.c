#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <kernel/arch/x86_64/apic.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>
#include <kernel/semaphore.h>
#include <kernel/spinlock.h>
#include <kernel/heap.h>

#define IRQ_MAX_CPUS 16
#define IRQ_DISPATCH_MAX_DEPTH 16

typedef struct irq_subscription {
  irq_handler_fn handler;
  void* ctx;
  struct irq_line* line;
  struct irq_subscription* next;
  bool removed;
  uint32_t active_calls;
  bool waiting_cleanup;
  ksem_t cleanup_sem;
} irq_subscription_t;

typedef struct irq_line {
  bool in_use;
  uint32_t irq;
  uint8_t vector;
  bool configured;
  bool needs_config;
  bool needs_cleanup;
  uint32_t handler_count;
  uint32_t dispatch_depth;
  irq_subscription_t* handlers;
  bool level_triggered;
  bool active_low;
} irq_line_t;

struct irq_handle {
  irq_subscription_t* subscription;
};

extern void (*irq_vector_stubs[IRQ_VECTOR_COUNT])(void);

static irq_line_t irq_lines[IRQ_VECTOR_COUNT];
static spinlock_t irq_lock;
static bool irq_system_initialized = false;
static bool irq_apic_ready = false;
static irq_subscription_t* irq_dispatch_stack[IRQ_MAX_CPUS][IRQ_DISPATCH_MAX_DEPTH];
static size_t irq_dispatch_stack_depth[IRQ_MAX_CPUS];

static inline uint32_t irq_current_cpu_index(void) {
  uint32_t apic_id = apic_current_processor_id();
  return apic_id % IRQ_MAX_CPUS;
}

static inline irq_line_t* irq_find_line(uint32_t irq) {
  for(size_t index = 0; index < IRQ_VECTOR_COUNT; ++index) {
    irq_line_t* line = &irq_lines[index];
    if(line->in_use && line->irq == irq) {
      return line;
    }
  }
  return NULL;
}

static inline irq_line_t* irq_line_from_vector(uint8_t vector) {
  if(vector < IRQ_VECTOR_BASE || vector > IRQ_VECTOR_LIMIT) {
    return NULL;
  }
  return &irq_lines[vector - IRQ_VECTOR_BASE];
}

static void irq_line_reset(irq_line_t* line, uint8_t vector) {
  memset(line, 0, sizeof(*line));
  line->vector = vector;
  line->level_triggered = true;
  line->active_low = true;
}

static irq_line_t* irq_allocate_line(uint32_t irq) {
  for(size_t index = 0; index < IRQ_VECTOR_COUNT; ++index) {
    irq_line_t* line = &irq_lines[index];
    if(!line->in_use && line->handler_count == 0 && line->handlers == NULL) {
      irq_line_reset(line, (uint8_t)(IRQ_VECTOR_BASE + index));
      line->irq = irq;
      line->in_use = true;
      line->needs_config = true;
      return line;
    }
  }
  return NULL;
}

static void irq_line_disable(irq_line_t* line) {
  if(line == NULL) {
    return;
  }

  uint32_t last_irq = line->irq;
  if(line->in_use && line->configured) {
    apic_update_irq_mask(last_irq, true);
  }

  line->irq = 0;
  line->configured = false;
  line->needs_config = false;
  line->needs_cleanup = false;
  line->handler_count = 0;
  line->dispatch_depth = 0;
  line->handlers = NULL;
  line->in_use = false;
}

static bool irq_configure_line(irq_line_t* line) {
  if(line == NULL) {
    return false;
  }
  if(line->configured) {
    return true;
  }

  if(!irq_apic_ready) {
    line->needs_config = true;
    return true;
  }

  bool configured = apic_configure_irq(line->irq,
                                      line->vector,
                                      line->level_triggered,
                                      line->active_low);
  if(!configured) {
    serial_printf("irq: failed to configure GSI %u -> vector 0x%02x\n",
                  line->irq,
                  line->vector);
    return false;
  }

  line->configured = true;
  line->needs_config = false;
  return true;
}

static void pic_send_eoi(uint32_t irq) {
  if(irq >= 16) {
    return;
  }

  if(irq >= 8) {
    outb(PIC2_COMMAND_PORT, 0x20);
  }
  outb(PIC1_COMMAND_PORT, 0x20);
}

static void irq_prune_removed(irq_line_t* line) {
  if(line == NULL) {
    return;
  }

  irq_subscription_t** link = &line->handlers;
  while(*link != NULL) {
    irq_subscription_t* subscription = *link;
    if(subscription->removed && subscription->active_calls == 0) {
      *link = subscription->next;
      if(line->handler_count > 0) {
        line->handler_count--;
      }
      subscription->line = NULL;
      if(subscription->waiting_cleanup) {
        ksem_post(&subscription->cleanup_sem);
      } else {
        ksem_destroy(&subscription->cleanup_sem);
        kfree(subscription);
      }
      continue;
    }
    link = &subscription->next;
  }

  line->needs_cleanup = false;

  if(line->handlers == NULL) {
    irq_line_disable(line);
  }
}

void irq_initialize(void) {
  if(irq_system_initialized) {
    return;
  }

  spinlock_init(&irq_lock);

  for(size_t index = 0; index < IRQ_VECTOR_COUNT; ++index) {
    irq_line_reset(&irq_lines[index], (uint8_t)(IRQ_VECTOR_BASE + index));
    idt_add_isr((int)(IRQ_VECTOR_BASE + index), (void*)irq_vector_stubs[index]);
  }

  irq_system_initialized = true;
}

void irq_apic_online(void) {
  spinlock_lock(&irq_lock);
  irq_apic_ready = true;

  for(size_t index = 0; index < IRQ_VECTOR_COUNT; ++index) {
    irq_line_t* line = &irq_lines[index];
    if(line->in_use && line->needs_config) {
      if(!irq_configure_line(line)) {
        // Leave the line marked as needing configuration so we can retry later.
        serial_printf("irq: pending configuration for GSI %u deferred\n", line->irq);
      }
    }
  }

  spinlock_unlock(&irq_lock);
}

int irq_register(uint32_t irq,
                 irq_handler_fn handler,
                 void* ctx,
                 const irq_config_t* config,
                 struct irq_handle** out_handle) {
  if(handler == NULL || out_handle == NULL) {
    return -EINVAL;
  }

  irq_subscription_t* subscription = kmalloc(sizeof(*subscription));
  if(subscription == NULL) {
    return -ENOMEM;
  }

  struct irq_handle* handle = kmalloc(sizeof(*handle));
  if(handle == NULL) {
    kfree(subscription);
    return -ENOMEM;
  }

  subscription->handler = handler;
  subscription->ctx = ctx;
  subscription->line = NULL;
  subscription->next = NULL;
  subscription->removed = false;
  subscription->active_calls = 0;
  subscription->waiting_cleanup = false;
  ksem_initialize(&subscription->cleanup_sem, 0);

  spinlock_lock(&irq_lock);

  irq_line_t* line = irq_find_line(irq);
  if(line == NULL) {
    line = irq_allocate_line(irq);
    if(line != NULL && config != NULL) {
      line->level_triggered = config->level_triggered;
      line->active_low = config->active_low;
    }
  } else if(config != NULL) {
    if(line->level_triggered != config->level_triggered ||
       line->active_low != config->active_low) {
      spinlock_unlock(&irq_lock);
      kfree(handle);
      kfree(subscription);
      return -EINVAL;
    }
  }

  if(line == NULL) {
    spinlock_unlock(&irq_lock);
    kfree(handle);
    kfree(subscription);
    return -ENOSPC;
  }

  subscription->line = line;
  subscription->next = line->handlers;
  line->handlers = subscription;
  line->handler_count++;

  if(!line->configured) {
    if(!irq_configure_line(line)) {
      line->handlers = subscription->next;
      line->handler_count--;
      if(line->handler_count == 0) {
        irq_line_disable(line);
      }
      spinlock_unlock(&irq_lock);
      kfree(handle);
      kfree(subscription);
      return -EIO;
    }
  }

  handle->subscription = subscription;

  spinlock_unlock(&irq_lock);

  *out_handle = handle;
  return 0;
}

int irq_unregister(struct irq_handle* handle) {
  if(handle == NULL || handle->subscription == NULL) {
    return -EINVAL;
  }

  irq_subscription_t* subscription = handle->subscription;
  irq_line_t* line = subscription->line;
  bool wait_needed = false;
  bool in_dispatch = false;
  uint32_t cpu_index = irq_current_cpu_index();

  spinlock_lock(&irq_lock);

  if(line == NULL || !line->in_use) {
    spinlock_unlock(&irq_lock);
    return -EINVAL;
  }

  for(size_t i = 0; i < irq_dispatch_stack_depth[cpu_index]; ++i) {
    if(irq_dispatch_stack[cpu_index][i] == subscription) {
      in_dispatch = true;
      break;
    }
  }

  subscription->removed = true;
  subscription->handler = NULL;

  line->needs_cleanup = true;

  if(subscription->active_calls == 0 && line->dispatch_depth == 0) {
    irq_prune_removed(line);
  } else if(!in_dispatch) {
    subscription->waiting_cleanup = true;
    wait_needed = true;
  }

  spinlock_unlock(&irq_lock);

  handle->subscription = NULL;

  if(wait_needed) {
    ksem_wait(&subscription->cleanup_sem);
    ksem_destroy(&subscription->cleanup_sem);
    kfree(subscription);
  }

  kfree(handle);
  return 0;
}

void irq_dispatch(uint8_t vector) {
  irq_line_t* line = irq_line_from_vector(vector);
  if(line == NULL || !line->in_use) {
    apic_send_eoi();
    return;
  }

  spinlock_lock(&irq_lock);
  uint32_t gsi = line->irq;
  uint32_t cpu_index = irq_current_cpu_index();
  line->dispatch_depth++;
  irq_subscription_t* current = line->handlers;
  spinlock_unlock(&irq_lock);

  bool handled_any = false;

  while(current != NULL) {
    irq_subscription_t* executing = current;
    irq_subscription_t* next;
    irq_handler_fn handler = NULL;
    void* ctx = NULL;
    bool pushed = false;

    spinlock_lock(&irq_lock);
    next = executing->next;
    handler = executing->handler;
    ctx = executing->ctx;
    if(handler != NULL) {
      if(irq_dispatch_stack_depth[cpu_index] < IRQ_DISPATCH_MAX_DEPTH) {
        irq_dispatch_stack[cpu_index][irq_dispatch_stack_depth[cpu_index]++] = executing;
        pushed = true;
        executing->active_calls++;
      } else {
        serial_printf("irq: dispatch stack overflow (GSI %u)\n", gsi);
        handler = NULL;
      }
    }
    spinlock_unlock(&irq_lock);

    bool handled = false;
    if(handler != NULL) {
      handled = handler(ctx);
    }

    spinlock_lock(&irq_lock);
    if(handler != NULL) {
      if(executing->active_calls > 0) {
        executing->active_calls--;
      }
      if(pushed) {
        if(irq_dispatch_stack_depth[cpu_index] > 0 &&
           irq_dispatch_stack[cpu_index][irq_dispatch_stack_depth[cpu_index] - 1] == executing) {
          irq_dispatch_stack[cpu_index][irq_dispatch_stack_depth[cpu_index] - 1] = NULL;
          irq_dispatch_stack_depth[cpu_index]--;
        } else {
          for(size_t i = 0; i < irq_dispatch_stack_depth[cpu_index]; ++i) {
            if(irq_dispatch_stack[cpu_index][i] == executing) {
              for(size_t j = i + 1; j < irq_dispatch_stack_depth[cpu_index]; ++j) {
                irq_dispatch_stack[cpu_index][j - 1] = irq_dispatch_stack[cpu_index][j];
              }
              irq_dispatch_stack[cpu_index][irq_dispatch_stack_depth[cpu_index] - 1] = NULL;
              irq_dispatch_stack_depth[cpu_index]--;
              break;
            }
          }
        }
      }
      if(handled) {
        handled_any = true;
      }
    }
    if(executing->removed && executing->active_calls == 0) {
      line->needs_cleanup = true;
    }
    current = next;
    spinlock_unlock(&irq_lock);
  }

  bool has_handlers = false;
  spinlock_lock(&irq_lock);
  if(line->dispatch_depth > 0) {
    line->dispatch_depth--;
  }
  if(line->dispatch_depth == 0 && line->needs_cleanup) {
    irq_prune_removed(line);
  }
  has_handlers = (line->handler_count > 0);
  spinlock_unlock(&irq_lock);

  if(gsi < 16) {
    pic_send_eoi(gsi);
  }
  apic_send_eoi();

  if(!handled_any && has_handlers) {
    serial_printf("irq: vector 0x%02x (GSI %u) was unhandled\n", vector, gsi);
  }
}
