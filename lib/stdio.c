#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86.h>

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
  outb(VGA_CTRL, VGA_HIGH_CURSOR);
  io_wait();
  pos |= inb(VGA_DATA);
  outb(VGA_CTRL, VGA_LOW_CURSOR);
  io_wait();
  pos |= ((uint16_t)inb(VGA_DATA)) << 8;
  return pos;
}

static void set_cursor_position(uint16_t pos) {
  outb(VGA_CTRL, VGA_HIGH_CURSOR);
  io_wait();
  outb(VGA_DATA, (uint8_t) (pos & 0xff));
  io_wait();
  outb(VGA_CTRL, VGA_LOW_CURSOR);
  io_wait();
  outb(VGA_DATA, (uint8_t) ((pos >> 8) & 0xff));
  io_wait();
}

int putchar(int ch) {
  const short color = 0x0700;
  short* vga = (short*)VGA_MEMORY;
  uint16_t pos = get_cursor_position();

  if(ch == '\n') {
    pos -= (pos % CRT_COLS);
    pos += CRT_COLS;
  // } if(ch == '\b') {
  //   vga[--pos] = color | ' ';
  } else {
    vga[pos++] = color | ch;
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

int puts(const char *s) {
  while(*s) putchar(*s++);
  return 1;
}
