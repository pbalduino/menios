#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>

#include <kernel/apic.h>
#include <kernel/condvar.h>
#include <kernel/console.h>
#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>
#include <kernel/tty.h>

static const uint16_t SERIAL_COM1_BASE = 0x3F8;

#define SERIAL_REG_DATA        0
#define SERIAL_REG_IER         1
#define SERIAL_REG_IIR         2
#define SERIAL_REG_FCR         2
#define SERIAL_REG_LCR         3
#define SERIAL_REG_MCR         4
#define SERIAL_REG_LSR         5
#define SERIAL_REG_MSR         6

#define SERIAL_LSR_DATA_READY  0x01

#define SERIAL_IRQ_GSI         4

#define SERIAL_RX_BUFFER_SIZE  4096

typedef struct serial_rx_state {
  bool       initialized;
  spinlock_t lock;
  kmutex_t   wait_lock;
  kcondvar_t data_available;
  uint8_t    buffer[SERIAL_RX_BUFFER_SIZE];
  size_t     head;
  size_t     tail;
} serial_rx_state_t;

bool serial_debug = false;

static spinlock_t serial_printf_lock;
static serial_rx_state_t serial_rx_state = {0};
static bool serial_irq_configured = false;

static inline uint8_t serial_port_in(uint8_t offset) {
  return inb((uint16_t)(SERIAL_COM1_BASE + offset));
}

static inline void serial_port_out(uint8_t offset, uint8_t value) {
  outb((uint16_t)(SERIAL_COM1_BASE + offset), value);
}

static void serial_buffer_init(void) {
  if(serial_rx_state.initialized) {
    return;
  }

  spinlock_init(&serial_rx_state.lock);
  kmutex_init(&serial_rx_state.wait_lock);
  kcondvar_init(&serial_rx_state.data_available);
  serial_rx_state.head = 0;
  serial_rx_state.tail = 0;
  serial_rx_state.initialized = true;
}

static inline bool serial_buffer_empty_locked(void) {
  return serial_rx_state.head == serial_rx_state.tail;
}

static bool serial_buffer_push(uint8_t ch) {
  bool notify;

  spinlock_lock(&serial_rx_state.lock);
  notify = serial_buffer_empty_locked();

  size_t next = (serial_rx_state.head + 1) % SERIAL_RX_BUFFER_SIZE;
  if(next == serial_rx_state.tail) {
    serial_rx_state.tail = (serial_rx_state.tail + 1) % SERIAL_RX_BUFFER_SIZE;
  }
  serial_rx_state.buffer[serial_rx_state.head] = ch;
  serial_rx_state.head = next;
  spinlock_unlock(&serial_rx_state.lock);
  return notify;
}

static size_t serial_buffer_pop_many(uint8_t* out, size_t max_length) {
  size_t count = 0;

  spinlock_lock(&serial_rx_state.lock);
  while(count < max_length && !serial_buffer_empty_locked()) {
    out[count++] = serial_rx_state.buffer[serial_rx_state.tail];
    serial_rx_state.tail = (serial_rx_state.tail + 1) % SERIAL_RX_BUFFER_SIZE;
  }
  spinlock_unlock(&serial_rx_state.lock);
  return count;
}

static void serial_acknowledge_irq(void) {
  (void)serial_port_in(SERIAL_REG_IIR);
  apic_send_eoi();
}

void serial_irq_handler(void) {
  serial_buffer_init();

  bool pushed = false;

  for(;;) {
    uint8_t iir = serial_port_in(SERIAL_REG_IIR);
    if(iir & 0x01u) {
      break; // No pending interrupts
    }

    uint8_t cause = (iir >> 1) & 0x07u;
    switch(cause) {
      case 0x02: // Received data available
      case 0x04: // Receiver timeout
      case 0x06: // Character timeout (16550A)
        while(serial_port_in(SERIAL_REG_LSR) & SERIAL_LSR_DATA_READY) {
          uint8_t ch = serial_port_in(SERIAL_REG_DATA);
          pushed = serial_buffer_push(ch) || pushed;
          tty_handle_input_char(ch);
        }
        break;
      case 0x03: // Line status
        (void)serial_port_in(SERIAL_REG_LSR);
        break;
      default:
        // Transmitter empty or modem status; ignore for now
        break;
    }
  }

  if(pushed) {
    kcondvar_broadcast(&serial_rx_state.data_available);
  }

  serial_acknowledge_irq();
}

void serial_init() {
  serial_buffer_init();

  // Disable interrupts while configuring
  serial_port_out(SERIAL_REG_IER, 0x00);

  // Set the baud rate (115200 bps)
  serial_port_out(SERIAL_REG_LCR, 0x80);     // Enable DLAB
  serial_port_out(SERIAL_REG_DATA, 0x03);    // Divisor low byte
  serial_port_out(SERIAL_REG_IER, 0x00);     // Divisor high byte
  serial_port_out(SERIAL_REG_LCR, 0x03);     // 8 data bits, 1 stop bit, no parity

  // Enable FIFO and modem control
  serial_port_out(SERIAL_REG_FCR, 0xC7);
  serial_port_out(SERIAL_REG_MCR, 0x0B);     // Enable DTR, RTS, OUT2 (interrupts)

  // Flush any stale state
  (void)serial_port_in(SERIAL_REG_LSR);
  (void)serial_port_in(SERIAL_REG_DATA);
  (void)serial_port_in(SERIAL_REG_IIR);
  (void)serial_port_in(SERIAL_REG_MSR);

  spinlock_init(&serial_printf_lock);
  serial_puts("- Initing serial communication\n");
  serial_puts(".OK\n");
}

void serial_enable_irq(void) {
  serial_buffer_init();

  if(serial_irq_configured) {
    serial_port_out(SERIAL_REG_IER, 0x01);
    return;
  }

  // Ensure modem control lines stay asserted
  serial_port_out(SERIAL_REG_MCR, 0x0B);

  // Clear any pending status
  (void)serial_port_in(SERIAL_REG_LSR);
  (void)serial_port_in(SERIAL_REG_DATA);
  (void)serial_port_in(SERIAL_REG_IIR);
  (void)serial_port_in(SERIAL_REG_MSR);

  // Route IRQ4 through the IOAPIC
  if(!apic_configure_irq(SERIAL_IRQ_GSI, ISR_SERIAL, false, false)) {
    serial_puts("serial_enable_irq: failed to route COM1 interrupt\n");
    serial_port_out(SERIAL_REG_IER, 0x00);
    return;
  }

  // Unmask IRQ4 on the PIC for legacy passthrough
  uint8_t pic_mask = inb(PIC1_DATA_PORT);
  pic_mask &= (uint8_t)~(1u << 4);
  outb(PIC1_DATA_PORT, pic_mask);

  serial_port_out(SERIAL_REG_IER, 0x01);
  serial_irq_configured = true;
}

int serial_putchar(int ch) {
  while((serial_port_in(SERIAL_REG_LSR) & 0x20u) == 0) {
    // Spin until the transmit holding register is ready
  }

  serial_port_out(SERIAL_REG_DATA, (uint8_t)(ch & 0xFFu));
  return 0;
}

int serial_puts(const char* text) {
  while(*text) {
    serial_putchar(*text++);
  }
  return 0;
}

int serial_vprintf(const char *format, va_list args){
  char buffer[1024];
  int len = vsprintk(buffer, format, args);

  uint64_t flags = spinlock_lock_irqsave(&serial_printf_lock);
  serial_puts(buffer);
  spinlock_unlock_irqrestore(&serial_printf_lock, flags);

  return len;
}

int serial_printf(const char* format, ...) {
  if(!serial_debug) {
    return 0;
  }

  va_list list;
  va_start(list, format);
  int result = serial_vprintf(format, list);
  va_end(list);
  return result;
}

int64_t serial_read(void* buffer, size_t length) {
  if(buffer == NULL || length == 0) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  serial_buffer_init();

  uint8_t* out = (uint8_t*)buffer;
  size_t total = 0;

  kmutex_lock(&serial_rx_state.wait_lock);
  while(total == 0) {
    total += serial_buffer_pop_many(out + total, length - total);
    if(total > 0) {
      break;
    }
    kcondvar_wait(&serial_rx_state.data_available, &serial_rx_state.wait_lock);
  }
  kmutex_unlock(&serial_rx_state.wait_lock);

  if(current) {
    current->errno = 0;
  }

  return (int64_t)total;
}
