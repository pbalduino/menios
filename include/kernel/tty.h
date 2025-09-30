#ifndef MENIOS_INCLUDE_KERNEL_TTY_H
#define MENIOS_INCLUDE_KERNEL_TTY_H

#include <stddef.h>
#include <stdint.h>

#include <kernel/file.h>

void tty_system_init(void);
file_t* tty_device_open(void);
void tty_handle_input_char(uint8_t ch);
void tty_push_bytes(const uint8_t* data, size_t length);

/* Temporary compatibility hook for legacy callers. */
static inline void stdin_enqueue_char(uint8_t ch) {
  tty_handle_input_char(ch);
}

#endif
