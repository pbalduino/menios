#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/errno.h>

#include <kernel/condvar.h>
#include <kernel/framebuffer.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>
#include <kernel/tty.h>
#include <kernel/vga_text.h>

#define TTY_INPUT_BUFFER_SIZE 4096
#define TTY_LINE_BUFFER_SIZE 512

typedef struct tty_state_t {
  bool        initialized;
  spinlock_t  buffer_lock;
  kmutex_t    wait_lock;
  kcondvar_t  data_available;
  uint8_t     buffer[TTY_INPUT_BUFFER_SIZE];
  size_t      head;
  size_t      tail;
  char        line[TTY_LINE_BUFFER_SIZE];
  size_t      line_length;
} tty_state_t;

static tty_state_t default_tty;

static int tty_char_open(char_device_t* device, uint32_t mode, file_t** out_file) {
  (void)device;
  if((mode & (FILE_MODE_READ | FILE_MODE_WRITE)) == 0) {
    mode |= FILE_MODE_READ;
  }

  file_t* file = tty_device_open();
  if(file == NULL) {
    return -ENOMEM;
  }
  file->mode = mode;
  *out_file = file;
  return 0;
}

static const char_device_ops_t tty_char_ops = {
  .open = tty_char_open,
};

static char_device_t tty_char_device_instance = {
  .name = "/dev/tty0",
  .supported_modes = FILE_MODE_READ | FILE_MODE_WRITE,
  .ops = &tty_char_ops,
  .driver_ctx = NULL,
  .next = NULL,
};

char_device_t* tty_char_device(void) {
  return &tty_char_device_instance;
}

static void tty_output_char(char ch) {
  vga_text_putc(ch);
  fb_putchar((uint8_t)ch);
  serial_port_putchar(SERIAL_PORT_CONSOLE, (uint8_t)ch);
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
}

void tty_system_init(void) {
  if(default_tty.initialized) {
    return;
  }

  vga_text_init();
  spinlock_init(&default_tty.buffer_lock);
  kmutex_init(&default_tty.wait_lock);
  kcondvar_init(&default_tty.data_available);
  default_tty.head = 0;
  default_tty.tail = 0;
  default_tty.line_length = 0;
  default_tty.initialized = true;
}

void tty_handle_input_char(uint8_t ch) {
  if(!default_tty.initialized) {
    tty_system_init();
  }

  if(ch == '\r') {
    ch = '\n';
  }

  bool notify = false;
  char out_seq[3];
  size_t out_len = 0;

  spinlock_lock(&default_tty.buffer_lock);

  if(ch == '\b' || ch == 0x7f) {
    if(default_tty.line_length > 0) {
      default_tty.line_length--;
      out_seq[0] = '\b';
      out_seq[1] = ' ';
      out_seq[2] = '\b';
      out_len = 3;
    }
  } else if(ch == 0x03) {
    default_tty.line_length = 0;
    out_seq[0] = '^';
    out_seq[1] = 'C';
    out_seq[2] = '\n';
    out_len = 3;
    tty_queue_push_byte_locked(&default_tty, '\n');
    notify = true;
  } else if(ch == '\n') {
    if(default_tty.line_length < (TTY_LINE_BUFFER_SIZE - 1)) {
      default_tty.line[default_tty.line_length++] = '\n';
    }
    out_seq[0] = '\n';
    out_len = 1;
    tty_commit_line_locked(&default_tty);
    notify = true;
  } else if(ch >= 0x20 && ch < 0x7f) {
    if(default_tty.line_length < (TTY_LINE_BUFFER_SIZE - 1)) {
      default_tty.line[default_tty.line_length++] = (char)ch;
      out_seq[0] = (char)ch;
      out_len = 1;
    } else {
    }
  }

  spinlock_unlock(&default_tty.buffer_lock);

  for(size_t idx = 0; idx < out_len; idx++) {
    tty_output_char(out_seq[idx]);
  }

  if(notify) {
    kcondvar_broadcast(&default_tty.data_available);
  }
}

void tty_push_bytes(const uint8_t* data, size_t length) {
  if(!default_tty.initialized) {
    tty_system_init();
  }

  spinlock_lock(&default_tty.buffer_lock);
  for(size_t idx = 0; idx < length; idx++) {
    tty_queue_push_byte_locked(&default_tty, data[idx]);
  }
  spinlock_unlock(&default_tty.buffer_lock);
  kcondvar_broadcast(&default_tty.data_available);
}

static int64_t tty_read_common(void* buffer, size_t length) {
  if(!default_tty.initialized) {
    tty_system_init();
  }

  if(buffer == NULL || length == 0) {
    if(current) {
      current->errno = EINVAL;
    }
    return -EINVAL;
  }

  uint8_t* out = (uint8_t*)buffer;
  size_t total = 0;

  kmutex_lock(&default_tty.wait_lock);
  while(total == 0) {
    serial_poll(SERIAL_PORT_CONSOLE);

    spinlock_lock(&default_tty.buffer_lock);
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
    spinlock_unlock(&default_tty.buffer_lock);

    if(total > 0) {
      break;
    }

    serial_poll(SERIAL_PORT_CONSOLE);

    kcondvar_wait(&default_tty.data_available, &default_tty.wait_lock);
  }
  kmutex_unlock(&default_tty.wait_lock);

  if(current) {
    current->errno = 0;
  }
  return (int64_t)total;
}

static int64_t tty_read_impl(file_t* file, void* buffer, size_t length) {
  (void)file;
  return tty_read_common(buffer, length);
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

int64_t tty_read(void* buffer, size_t length) {
  return tty_read_common(buffer, length);
}
