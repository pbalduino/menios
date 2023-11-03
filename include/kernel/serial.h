#ifndef MENIOS_INCLUDE_KERNEL_SERIAL_H
#define MENIOS_INCLUDE_KERNEL_SERIAL_H 1

void serial_init();
int serial_putchar(int ch);
int serial_puts(const char* text);
int serial_printf(const char* format, ...);

#define serial_log(a) serial_printf("[INFO] %s[%d]: %s\n", __FILE__, __LINE__, a)
#define serial_error(a) serial_printf("[ERRO] %s[%d]: %s\n", __FILE__, __LINE__, a)

#endif
