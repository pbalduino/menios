#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/errno.h>

#include <kernel/condvar.h>
#include <kernel/framebuffer.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/tty.h>

#define TTY_INPUT_BUFFER_SIZE 4096
#define TTY_LINE_BUFFER_SIZE 512

typedef struct tty_state_t {
  bool       initialized;
  kmutex_t   lock;
  kcondvar_t data_available;
  uint8_t    buffer[TTY_INPUT_BUFFER_SIZE];
  size_t     head;
  size_t     tail;
  char       line[TTY_LINE_BUFFER_SIZE];
  size_t     line_length;
} tty_state_t;

static tty_state_t default_tty;

static void tty_output_char(char ch) {
  fb_putchar((uint8_t)ch);
  serial_putchar((uint8_t)ch);
}

static void tty_output_string(const char* text) {
  while(*text) {
    tty_output_char(*text++);
  }
}

static void tty_queue_push_byte_locked(tty_state_t* tty, uint8_t ch) {
  size_t next = (tty->head + 1) % TTY_INPUT_BUFFER_SIZE;
  if(next == tty->tail) {
    tty->tail = (tty->tail + 1) % TTY_INPUT_BUFFER_SIZE;
  }
  tty->buffer[tty->head] = ch;
  tty->head = next;
}

static bool tty_queue_pop_byte_locked(tty_state_t* tty, uint8_t* out) {
  if(tty->head == tty->tail) {
    return false;
  }
  *out = tty->buffer[tty->tail];
  tty->tail = (tty->tail + 1) % TTY_INPUT_BUFFER_SIZE;
  return true;
}

static void tty_commit_line_locked(tty_state_t* tty) {
  for(size_t idx = 0; idx < tty->line_length; idx++) {
    tty_queue_push_byte_locked(tty, (uint8_t)tty->line[idx]);
  }
  tty->line_length = 0;
  kcondvar_broadcast(&tty->data_available);
}

void tty_system_init(void) {
  if(default_tty.initialized) {
    return;
  }

  kmutex_init(&default_tty.lock);
  kcondvar_init(&default_tty.data_available);
  default_tty.head = 0;
  default_tty.tail = 0;
  default_tty.line_length = 0;
  default_tty.initialized = true;
}

static void tty_handle_backspace(tty_state_t* tty) {
  if(tty->line_length == 0) {
    return;
  }
  tty->line_length--;
  tty_output_string("\b \b");
}

static void tty_handle_newline_locked(tty_state_t* tty) {
  if(tty->line_length < (TTY_LINE_BUFFER_SIZE - 1)) {
    tty->line[tty->line_length++] = '\n';
  }
  tty_output_char('\n');
  tty_commit_line_locked(tty);
}

static void tty_handle_control_c_locked(tty_state_t* tty) {
  tty->line_length = 0;
  tty_output_string("^C\n");
  tty_queue_push_byte_locked(tty, '\n');
  kcondvar_broadcast(&tty->data_available);
}

void tty_handle_input_char(uint8_t ch) {
  if(!default_tty.initialized) {
    tty_system_init();
  }

  if(ch == '\r') {
    ch = '\n';
  }

  kmutex_lock(&default_tty.lock);

  if(ch == '\b' || ch == 0x7f) {
    tty_handle_backspace(&default_tty);
    kmutex_unlock(&default_tty.lock);
    return;
  }

  if(ch == 0x03) {
    tty_handle_control_c_locked(&default_tty);
    kmutex_unlock(&default_tty.lock);
    return;
  }

  if(ch == '\n') {
    tty_handle_newline_locked(&default_tty);
    kmutex_unlock(&default_tty.lock);
    return;
  }

  if(ch < 0x20 || ch >= 0x7f) {
    kmutex_unlock(&default_tty.lock);
    return;
  }

  if(default_tty.line_length < (TTY_LINE_BUFFER_SIZE - 1)) {
    default_tty.line[default_tty.line_length++] = (char)ch;
    tty_output_char((char)ch);
  }

  kmutex_unlock(&default_tty.lock);
}

static int64_t tty_read_impl(file_t* file, void* buffer, size_t length) {
  (void)file;

  if(buffer == NULL || length == 0) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  uint8_t* out = (uint8_t*)buffer;
  size_t total = 0;

  kmutex_lock(&default_tty.lock);
  while(total == 0) {
    while(total < length) {
      uint8_t ch;
      if(!tty_queue_pop_byte_locked(&default_tty, &ch)) {
        break;
      }
      out[total++] = ch;
      if(ch == '\n') {
        break;
      }
    }

    if(total > 0) {
      break;
    }

    kcondvar_wait(&default_tty.data_available, &default_tty.lock);
  }
  kmutex_unlock(&default_tty.lock);

  if(current) {
    current->errno = 0;
  }
  return (int64_t)total;
}

static int64_t tty_write_impl(file_t* file, const void* buffer, size_t length) {
  (void)file;
  if(buffer == NULL) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  const uint8_t* data = (const uint8_t*)buffer;
  for(size_t idx = 0; idx < length; idx++) {
    uint8_t ch = data[idx];
    tty_output_char((char)ch);
  }
  if(current) {
    current->errno = 0;
  }
  return (int64_t)length;
}

static const file_ops_t tty_file_ops = {
  .read = tty_read_impl,
  .write = tty_write_impl,
  .close = NULL,
  .seek = NULL,
};

file_t* tty_device_open(void) {
  if(!default_tty.initialized) {
    tty_system_init();
  }

  file_t* file = file_create(&tty_file_ops, NULL, FILE_MODE_READ | FILE_MODE_WRITE);
  if(file == NULL && current != NULL && current->errno == 0) {
    current->errno = ENOMEM;
  }
  return file;
}
