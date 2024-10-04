#ifndef MENIOS_INCLUDE_KERNEL_SERIAL_H
#define MENIOS_INCLUDE_KERNEL_SERIAL_H

#include <stdarg.h>

#ifdef MENIOS_KERNEL
void serial_init();
int serial_putchar(int ch);
int serial_puts(const char* text);
int serial_printf(const char* format, ...);
int serial_vprintf(const char *format, va_list args);

#define serial_line(a) serial_printf("%s: [%s:%d] %s\n", __func__, __FILE__, __LINE__, a)
#define serial_log(a) serial_printf("[INFO] %s[%d]: %s\n", __FILE__, __LINE__, a)
#define serial_error(a) serial_printf("[ERRO] %s[%d]: %s\n", __FILE__, __LINE__, a)

#else
#ifdef MENIOS_NO_DEBUG
  #define serial_init()
  #define serial_putchar(a)
  #define serial_puts(a)
  #define serial_printf(fmt, ...)
  #define serial_printf(fmt, ...)

  #define serial_log(a)
  #define serial_line(a)
  #define serial_error(a)
#else
  #warning Calling printf as serial_printf
  #define serial_init()
  #define serial_putchar(a)          putchar(a)
  #define serial_puts(a)             puts(a)
  #define serial_printf(fmt, ...)    printf(fmt, ##__VA_ARGS__)

  #define serial_log(a)
  #define serial_line(a)
  #define serial_error(a)

#endif // MENIOS_NO_DEBUG
#endif // MENIOS_KERNEL

// #define serial_line(a
// #define serial_log(a) serial_printf("[INFO] %s[%d]: %s\n", __FILE__, __LINE__, a)
// #define serial_error(a) serial_printf("[ERRO] %s[%d]: %s\n", __FILE__, __LINE__, a)

#endif
