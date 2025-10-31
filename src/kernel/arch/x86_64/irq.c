#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <kernel/arch/x86_64/apic.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>
#include <kernel/heap.h>

typedef struct irq_subscription {
  irq_handler_fn handler;
  void* ctx;
  struct irq_line* line;
  struct irq_subscription* next;
  bool removed;
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
} irq_line_t;

struct irq_handle {
  irq_subscription_t* subscription;
};

extern void (*irq_vector_stubs[IRQ_VECTOR_COUNT])(void);

static irq_line_t irq_lines[IRQ_VECTOR_COUNT];
static spinlock_t irq_lock;
static bool irq_system_initialized = false;
static bool irq_apic_ready = false;

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

  bool configured = apic_configure_irq(line->irq, line->vector, true, true);
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
    if(subscription->removed) {
      *link = subscription->next;
      if(line->handler_count > 0) {
        line->handler_count--;
      }
      subscription->line = NULL;
      kfree(subscription);
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

  spinlock_lock(&irq_lock);

  irq_line_t* line = irq_find_line(irq);
  if(line == NULL) {
    line = irq_allocate_line(irq);
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

  spinlock_lock(&irq_lock);

  if(line == NULL || !line->in_use) {
    spinlock_unlock(&irq_lock);
    return -EINVAL;
  }

  subscription->removed = true;
  subscription->handler = NULL;

  if(line->dispatch_depth == 0) {
    irq_prune_removed(line);
  } else {
    line->needs_cleanup = true;
  }

  spinlock_unlock(&irq_lock);

  handle->subscription = NULL;
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
  line->dispatch_depth++;
  irq_subscription_t* subscription = line->handlers;
  spinlock_unlock(&irq_lock);

  while(subscription != NULL) {
    irq_handler_fn handler;
    void* ctx;
    irq_subscription_t* next;

    spinlock_lock(&irq_lock);
    handler = subscription->handler;
    ctx = subscription->ctx;
    next = subscription->next;
    spinlock_unlock(&irq_lock);

    if(handler != NULL) {
      handler(ctx);
    }

    subscription = next;
  }

  spinlock_lock(&irq_lock);
  if(line->dispatch_depth > 0) {
    line->dispatch_depth--;
  }
  if(line->dispatch_depth == 0 && line->needs_cleanup) {
    irq_prune_removed(line);
  }
  spinlock_unlock(&irq_lock);

  pic_send_eoi(line->irq);
  apic_send_eoi();
}
