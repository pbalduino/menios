#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86.h>

#define VGA_MEMORY 0xb8000
// FIXME: read from bios the current width
#define TEXT_SCREEN_WIDTH 80

uint16_t get_cursor_position(void) {
  uint16_t pos = 0;
  outb(0x3d4, 0x0f);
  pos |= inb(0x3d5);
  outb(0x3d4, 0x0e);
  pos |= ((uint16_t)inb(0x3d5)) << 8;
  return pos;
}

void set_cursor_position(uint16_t pos) {
  outb(0x3d4, 0x0f);
	outb(0x3d5, (uint8_t) (pos & 0xff));
	outb(0x3d4, 0x0e);
	outb(0x3d5, (uint8_t) ((pos >> 8) & 0xff));
}

int putchar(int ch) {
  const short color = 0x0700;
  short* vga = (short*)VGA_MEMORY;
  uint16_t pos = get_cursor_position();

  if(ch == '\n') {
    pos = ((pos / TEXT_SCREEN_WIDTH) + 1) * TEXT_SCREEN_WIDTH;
  } else {
    vga[pos++] = color | ch;
  }

  set_cursor_position(pos);

  return ch;
}

int puts(const char *s) {
  while(*s) putchar(*s++);
  return 1;
}

int vprintf(const char* format, va_list args) {
  for(int pos = 0; format[pos]; pos++) {
    if(format[pos] == '%') {
      switch(format[++pos]) {
        case 'c': {
          const int val = va_arg(args, int);
          putchar(val);
          break;
        }
        case 'i':
        case 'd': {
          int val = va_arg(args, int);
          char* str = "";
          itoa(val, str, 10);
          puts(str);
          break;
        }
        case 'u': {
          int val = va_arg(args, unsigned);
          char* str = "";
          itoa(val, str, 10);
          puts(str);
          break;
        }
        case 's': {
          const char* val = (const char*)va_arg(args, char*);
          puts(val);
          break;
        }
        case 'x': {
          int val = va_arg(args, unsigned);
          char* str = "";
          itoa(val, str, 16);
          puts(str);
          break;
        }
        default: {
          putchar('?');
          putchar(format[pos]);
          putchar('?');
        }
      }
    } else {
      putchar(format[pos]);
    }
  }
  return 0;
}

__attribute__ ((format (printf, 1, 2))) int printf (const char* format, ...) {
  va_list list;
  va_start (list, format);
  int i = vprintf(format, list);
  va_end(list);
  return i;
}
