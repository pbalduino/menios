#ifndef MENIOS_INCLUDE_STDIO_H
#define MENIOS_INCLUDE_STDIO_H

#include <stdarg.h>
#include <types.h>

#define FLAG_LEFT  0x01 // The '-' flag
#define FLAG_SIGN  0x02 // The '+' flag
#define FLAG_SPACE 0x04 // The ' ' flag
#define FLAG_TYPE  0x08 // The '#' flag
#define FLAG_ZERO  0x10 // The '0' flag

int getchar();

char* gets(char* str);

int putchar(int ch);

int puts(const char *s);

int	vprintf(const char*, va_list);

__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...);

#endif
