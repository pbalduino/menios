#include <kernel/console.h>

#include <arch.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


#define VGA_MEMORY 0xb8000
#define VGA_CTRL 0x3d4
#define VGA_DATA 0x3d5

#define VGA_HIGH_CURSOR 0x0f
#define VGA_LOW_CURSOR 0x0e
// FIXME: read from bios the current width
#define CRT_COLS 80
#define CRT_ROWS 25
#define CRT_SIZE (CRT_COLS * CRT_ROWS)

static uint16_t get_cursor_position(void) {
  uint16_t pos = 0;
  outb(VGA_CTRL, VGA_LOW_CURSOR);
  pos |= ((uint16_t)inb(VGA_DATA)) << 8;
  outb(VGA_CTRL, VGA_HIGH_CURSOR);
  pos |= inb(VGA_DATA);
  return pos;
}

static void set_cursor_position(uint16_t pos) {
  outb(VGA_CTRL, VGA_HIGH_CURSOR);
  outb(VGA_DATA, (uint8_t) (pos & 0xff));
  outb(VGA_CTRL, VGA_LOW_CURSOR);
  outb(VGA_DATA, (uint8_t) ((pos >> 8) & 0xff));
}

int putchar(int ch) {
  const short color = 0x0700;
  uint16_t* vga = (uint16_t*)VGA_MEMORY;
  uint16_t pos = get_cursor_position();

  if(ch == '\n') {
    pos -= (pos % CRT_COLS);
    pos += CRT_COLS;
  } else {
    vga[pos++] = (uint16_t)(color | (uint8_t) (ch & 0xff));
  }

  if(pos >= CRT_SIZE) {
    int i;

    memmove(vga, vga + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
    for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++) {
      vga[i] = color | ' ';
    }
    pos -= CRT_COLS;
  }

  set_cursor_position(pos);

  return ch;
}

int puts(const char* s) {
  while(*s) putchar(*s++);
  return 1;
}
