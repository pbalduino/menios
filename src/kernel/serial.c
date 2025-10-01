#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>

#include <kernel/apic.h>
#include <kernel/condvar.h>
#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>
#include <kernel/tty.h>

#define SERIAL_MAX_PORTS SERIAL_PORT_COUNT

#define SERIAL_COM1_BASE 0x3F8
#define SERIAL_COM2_BASE 0x2F8

#define SERIAL_REG_DATA        0
#define SERIAL_REG_IER         1
#define SERIAL_REG_IIR         2
#define SERIAL_REG_FCR         2
#define SERIAL_REG_LCR         3
#define SERIAL_REG_MCR         4
#define SERIAL_REG_LSR         5
#define SERIAL_REG_MSR         6

#define SERIAL_LSR_DATA_READY  0x01

#define SERIAL_IRQ_COM1        4
#define SERIAL_IRQ_COM2        3

#define SERIAL_RX_BUFFER_SIZE  4096

#include <kernel/console.h>

bool serial_debug = false;

typedef struct serial_port_state {
  bool       present;
  uint16_t   io_base;
  uint8_t    irq_gsi;
  uint8_t    vector;
  bool       irq_configured;
  bool       feeds_tty;
  spinlock_t printf_lock;

  spinlock_t rx_lock;
  kmutex_t   rx_wait_lock;
  kcondvar_t rx_available;
  uint8_t    buffer[SERIAL_RX_BUFFER_SIZE];
  size_t     head;
  size_t     tail;
} serial_port_state_t;

static serial_port_state_t serial_ports[SERIAL_MAX_PORTS];

static inline serial_port_state_t* serial_port(serial_port_t port) {
  if(port < 0 || port >= SERIAL_MAX_PORTS) {
    return NULL;
  }
  return &serial_ports[port];
}

static inline uint8_t serial_port_in(serial_port_state_t* port, uint8_t offset) {
  return inb((uint16_t)(port->io_base + offset));
}

static inline void serial_port_out(serial_port_state_t* port, uint8_t offset, uint8_t value) {
  outb((uint16_t)(port->io_base + offset), value);
}

static void serial_buffer_init(serial_port_state_t* port) {
  if(port->present) {
    return;
  }

  spinlock_init(&port->rx_lock);
  kmutex_init(&port->rx_wait_lock);
  kcondvar_init(&port->rx_available);
  port->head = 0;
  port->tail = 0;
  port->present = true;
}

static inline bool serial_buffer_empty_locked(serial_port_state_t* port) {
  return port->head == port->tail;
}

static bool serial_buffer_push(serial_port_state_t* port, uint8_t ch) {
  bool notify;

  spinlock_lock(&port->rx_lock);
  notify = serial_buffer_empty_locked(port);

  size_t next = (port->head + 1) % SERIAL_RX_BUFFER_SIZE;
  if(next == port->tail) {
    port->tail = (port->tail + 1) % SERIAL_RX_BUFFER_SIZE;
  }
  port->buffer[port->head] = ch;
  port->head = next;
  spinlock_unlock(&port->rx_lock);
  return notify;
}

static size_t serial_buffer_pop_many(serial_port_state_t* port, uint8_t* out, size_t max_length) {
  size_t count = 0;

  spinlock_lock(&port->rx_lock);
  while(count < max_length && !serial_buffer_empty_locked(port)) {
    out[count++] = port->buffer[port->tail];
    port->tail = (port->tail + 1) % SERIAL_RX_BUFFER_SIZE;
  }
  spinlock_unlock(&port->rx_lock);
  return count;
}

static bool serial_drain_fifo(serial_port_state_t* port) {
  bool notify = false;

  while(serial_port_in(port, SERIAL_REG_LSR) & SERIAL_LSR_DATA_READY) {
    uint8_t ch = serial_port_in(port, SERIAL_REG_DATA);
    notify = serial_buffer_push(port, ch) || notify;
    if(port->feeds_tty) {
      tty_handle_input_char(ch);
    }
  }

  return notify;
}

static void serial_acknowledge_irq(serial_port_state_t* port) {
  (void)serial_port_in(port, SERIAL_REG_IIR);
  apic_send_eoi();
}

static void serial_handle_irq(serial_port_state_t* port) {
  serial_buffer_init(port);

  bool notify = false;

  for(;;) {
    uint8_t iir = serial_port_in(port, SERIAL_REG_IIR);
    if(iir & 0x01u) {
      break; // No pending interrupts
    }

    uint8_t cause = (iir >> 1) & 0x07u;
    switch(cause) {
      case 0x02: // Received data available
      case 0x04: // Receiver timeout
      case 0x06: // Character timeout (16550A)
        notify = serial_drain_fifo(port) || notify;
        break;
      case 0x03: // Line status
        (void)serial_port_in(port, SERIAL_REG_LSR);
        break;
      default:
        break;
    }
  }

  if(notify) {
    kcondvar_broadcast(&port->rx_available);
  }

  serial_acknowledge_irq(port);
}

void serial_irq_handler_com1(void) {
  serial_port_state_t* port = serial_port(SERIAL_PORT_COM1);
  if(port != NULL) {
    serial_handle_irq(port);
  }
}

void serial_irq_handler_com2(void) {
  serial_port_state_t* port = serial_port(SERIAL_PORT_COM2);
  if(port != NULL) {
    serial_handle_irq(port);
  }
}

void serial_system_init(void) {
  serial_port_state_t* com1 = serial_port(SERIAL_PORT_COM1);
  serial_port_state_t* com2 = serial_port(SERIAL_PORT_COM2);

  serial_buffer_init(com1);
  serial_buffer_init(com2);

  com1->io_base = SERIAL_COM1_BASE;
  com2->io_base = SERIAL_COM2_BASE;

  com1->irq_gsi = SERIAL_IRQ_COM1;
  com2->irq_gsi = SERIAL_IRQ_COM2;

  com1->vector = ISR_SERIAL_COM1;
  com2->vector = ISR_SERIAL_COM2;

  com1->feeds_tty = true;
  com2->feeds_tty = false;

  spinlock_init(&com1->printf_lock);
  spinlock_init(&com2->printf_lock);

  serial_port_out(com1, SERIAL_REG_IER, 0x00);
  serial_port_out(com1, SERIAL_REG_LCR, 0x80);
  serial_port_out(com1, SERIAL_REG_DATA, 0x03);
  serial_port_out(com1, SERIAL_REG_IER, 0x00);
  serial_port_out(com1, SERIAL_REG_LCR, 0x03);
  serial_port_out(com1, SERIAL_REG_FCR, 0xC7);
  serial_port_out(com1, SERIAL_REG_MCR, 0x0B);
  (void)serial_port_in(com1, SERIAL_REG_LSR);
  (void)serial_port_in(com1, SERIAL_REG_DATA);
  (void)serial_port_in(com1, SERIAL_REG_IIR);
  (void)serial_port_in(com1, SERIAL_REG_MSR);

  serial_port_out(com2, SERIAL_REG_IER, 0x00);
  serial_port_out(com2, SERIAL_REG_LCR, 0x80);
  serial_port_out(com2, SERIAL_REG_DATA, 0x03);
  serial_port_out(com2, SERIAL_REG_IER, 0x00);
  serial_port_out(com2, SERIAL_REG_LCR, 0x03);
  serial_port_out(com2, SERIAL_REG_FCR, 0xC7);
  serial_port_out(com2, SERIAL_REG_MCR, 0x0B);
  (void)serial_port_in(com2, SERIAL_REG_LSR);
  (void)serial_port_in(com2, SERIAL_REG_DATA);
  (void)serial_port_in(com2, SERIAL_REG_IIR);
  (void)serial_port_in(com2, SERIAL_REG_MSR);

  serial_port_puts(SERIAL_PORT_DEBUG, "- Serial COM1 ready\n");
  serial_port_puts(SERIAL_PORT_DEBUG, "- Serial COM2 ready\n");
}

void serial_enable_irq(serial_port_t which) {
  serial_port_state_t* port = serial_port(which);
  if(port == NULL) {
    return;
  }

  serial_buffer_init(port);

  if(port->irq_configured) {
    serial_port_out(port, SERIAL_REG_IER, 0x01);
    return;
  }

  serial_port_out(port, SERIAL_REG_MCR, 0x0B);
  (void)serial_port_in(port, SERIAL_REG_LSR);
  (void)serial_port_in(port, SERIAL_REG_DATA);
  (void)serial_port_in(port, SERIAL_REG_IIR);
  (void)serial_port_in(port, SERIAL_REG_MSR);

  if(!apic_configure_irq(port->irq_gsi, port->vector, false, false)) {
    serial_port_puts(which, "serial_enable_irq: failed to route interrupt\n");
    serial_port_out(port, SERIAL_REG_IER, 0x00);
    return;
  }

  uint8_t pic_mask = inb(PIC1_DATA_PORT);
  pic_mask &= (uint8_t)~(1u << port->irq_gsi);
  outb(PIC1_DATA_PORT, pic_mask);

  serial_port_out(port, SERIAL_REG_IER, 0x01);
  port->irq_configured = true;
}

void serial_poll(serial_port_t which) {
  serial_port_state_t* port = serial_port(which);
  if(port == NULL) {
    return;
  }
  serial_buffer_init(port);
  if(serial_drain_fifo(port)) {
    kcondvar_broadcast(&port->rx_available);
  }
}

int serial_port_putchar(serial_port_t which, int ch) {
  serial_port_state_t* port = serial_port(which);
  if(port == NULL) {
    return -ENODEV;
  }
  serial_buffer_init(port);

  while((serial_port_in(port, SERIAL_REG_LSR) & 0x20u) == 0) {
  }

  serial_port_out(port, SERIAL_REG_DATA, (uint8_t)(ch & 0xFFu));
  return 0;
}

int serial_port_puts(serial_port_t which, const char* text) {
  if(text == NULL) {
    return -EINVAL;
  }
  while(*text) {
    serial_port_putchar(which, *text++);
  }
  return 0;
}

int serial_port_vprintf(serial_port_t which, const char *format, va_list args) {
  serial_port_state_t* port = serial_port(which);
  if(port == NULL) {
    return -ENODEV;
  }

  char buffer[1024];
  int len = vsprintk(buffer, format, args);

  uint64_t flags = spinlock_lock_irqsave(&port->printf_lock);
  serial_port_puts(which, buffer);
  spinlock_unlock_irqrestore(&port->printf_lock, flags);

  return len;
}

int64_t serial_port_read(serial_port_t which, void* buffer, size_t length) {
  serial_port_state_t* port = serial_port(which);
  if(port == NULL) {
    if(current) {
      current->errno = ENODEV;
    }
    return -ENODEV;
  }
  serial_buffer_init(port);

  if(buffer == NULL || length == 0) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  uint8_t* out = (uint8_t*)buffer;
  size_t total = 0;

  kmutex_lock(&port->rx_wait_lock);
  while(total == 0) {
    total += serial_buffer_pop_many(port, out + total, length - total);
    if(total > 0) {
      break;
    }
    serial_drain_fifo(port);
    kcondvar_wait(&port->rx_available, &port->rx_wait_lock);
  }
  kmutex_unlock(&port->rx_wait_lock);

  if(current) {
    current->errno = 0;
  }

  return (int64_t)total;
}

int serial_port_printf(serial_port_t which, const char* format, ...) {
  va_list list;
  va_start(list, format);
  int result = serial_port_vprintf(which, format, list);
  va_end(list);
  return result;
}

int serial_vprintf(const char *format, va_list args) {
  if(!serial_debug) {
    return 0;
  }
  return serial_port_vprintf(SERIAL_PORT_DEBUG, format, args);
}

int serial_printf(const char* format, ...) {
  if(!serial_debug) {
    return 0;
  }
  va_list list;
  va_start(list, format);
  int result = serial_port_vprintf(SERIAL_PORT_DEBUG, format, list);
  va_end(list);
  return result;
}

*** End Patch
