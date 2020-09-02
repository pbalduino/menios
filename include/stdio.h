#ifndef MENIOS_INCLUDE_STDIO_H
#define MENIOS_INCLUDE_STDIO_H

#include <stdarg.h>
#include <types.h>

#define FLAG_LEFT  0x01 // The '-' flag
#define FLAG_SIGN  0x02 // The '+' flag
#define FLAG_SPACE 0x04 // The ' ' flag
#define FLAG_TYPE  0x08 // The '#' flag
#define FLAG_ZERO  0x10 // The '0' flag

int printf(const char *format, ...);

int putchar(int ch);

int puts(const char *s);

int	vprintf(const char*, va_list);

#endif
