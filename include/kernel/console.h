#ifndef MENIOS_INCLUDE_KERNEL_CONSOLE_H
#define MENIOS_INCLUDE_KERNEL_CONSOLE_H

#include <stdarg.h>
#include <stdint.h>

#define FLAG_LEFT  0x01 // The '-' flag
#define FLAG_SIGN  0x02 // The '+' flag
#define FLAG_SPACE 0x04 // The ' ' flag
#define FLAG_TYPE  0x08 // The '#' flag
#define FLAG_ZERO  0x10 // The '0' flag

void set_cursor_position(uint16_t pos);

uint16_t get_cursor_position();

#endif
