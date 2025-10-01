#ifndef MENIOS_INCLUDE_KERNEL_SERIAL_H
#define MENIOS_INCLUDE_KERNEL_SERIAL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef MENIOS_KERNEL
typedef enum {
  SERIAL_PORT_COM1 = 0,
  SERIAL_PORT_COM2 = 1,
  SERIAL_PORT_COUNT
} serial_port_t;

#define SERIAL_PORT_CONSOLE SERIAL_PORT_COM1
#define SERIAL_PORT_DEBUG   SERIAL_PORT_COM2

void serial_system_init(void);
void serial_enable_irq(serial_port_t port);
void serial_poll(serial_port_t port);

int serial_port_putchar(serial_port_t port, int ch);
int serial_port_puts(serial_port_t port, const char* text);
int serial_port_printf(serial_port_t port, const char* format, ...);
int serial_port_vprintf(serial_port_t port, const char *format, va_list args);
int64_t serial_port_read(serial_port_t port, void* buffer, size_t length);

static inline int serial_putchar(int ch) {
  return serial_port_putchar(SERIAL_PORT_DEBUG, ch);
}

static inline int serial_puts(const char* text) {
  return serial_port_puts(SERIAL_PORT_DEBUG, text);
}

int serial_printf(const char* format, ...);
int serial_vprintf(const char *format, va_list args);
static inline int64_t serial_read(void* buffer, size_t length) {
  return serial_port_read(SERIAL_PORT_CONSOLE, buffer, length);
}

#define serial_line(a) serial_printf("%s: [%s:%d] %s\n", __func__, __FILE__, __LINE__, a)
#define serial_log(a) serial_printf("[INFO] %s[%d]: %s\n", __FILE__, __LINE__, a)
#define serial_error(a) serial_printf("[ERRO] %s[%d]: %s\n", __FILE__, __LINE__, a)

#else
#ifdef MENIOS_NO_DEBUG
  #define serial_system_init()
  #define serial_enable_irq(port)
  #define serial_poll(port)
  #define serial_port_putchar(port, ch)
  #define serial_port_puts(port, text)
  #define serial_port_printf(port, fmt, ...)
  #define serial_port_vprintf(port, fmt, args)
  #define serial_port_read(port, buffer, length) (-1)
  #define serial_putchar(ch)
  #define serial_puts(text)
  #define serial_printf(fmt, ...)
  #define serial_vprintf(fmt, args)

  #define serial_log(a)
  #define serial_line(a)
  #define serial_error(a)
#else
  #warning Calling printf as serial_printf
  #define serial_system_init()
  #define serial_enable_irq(port)
  #define serial_poll(port)
  #define serial_port_putchar(port, ch)    putchar(ch)
  #define serial_port_puts(port, text)     puts(text)
  #define serial_port_printf(port, fmt, ...) printf(fmt, ##__VA_ARGS__)
  #define serial_port_vprintf(port, fmt, args) vprintf(fmt, args)
  #define serial_port_read(port, buffer, length) (-1)
  #define serial_putchar(ch)               putchar(ch)
  #define serial_puts(text)                puts(text)
  #define serial_printf(fmt, ...)          printf(fmt, ##__VA_ARGS__)
  #define serial_vprintf(fmt, args)        vprintf(fmt, args)

  #define serial_log(a)
  #define serial_line(a)
  #define serial_error(a)

#endif // MENIOS_NO_DEBUG
#endif // MENIOS_KERNEL

extern bool serial_debug;
// #define serial_line(a
// #define serial_log(a) serial_printf("[INFO] %s[%d]: %s\n", __FILE__, __LINE__, a)
// #define serial_error(a) serial_printf("[ERRO] %s[%d]: %s\n", __FILE__, __LINE__, a)

#endif
