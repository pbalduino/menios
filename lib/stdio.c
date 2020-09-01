#include <stdio.h>
#include <x86.h>

#define VGA_MEMORY 0xb8000

uint16_t get_cursor_position(void) {
  uint16_t pos = 0;
  outb(0x3d4, 0x0F);
  pos |= inb(0x3d5);
  outb(0x3d4, 0x0e);
  pos |= ((uint16_t)inb(0x3d5)) << 8;
  return pos;
}

void set_cursor_position(uint16_t pos) {
  outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

int putchar(int ch) {
  const short color = 0x0700;
  short* vga = (short*)VGA_MEMORY;
  uint16_t pos = get_cursor_position();

  if(ch == '\n') {
    pos = ((pos / 80) + 1) * 80;
  } else {
    vga[pos++] = color | ch;
  }

  set_cursor_position(pos);

  return ch;
}

int puts(const char *s) {
  int i = 0;

  while(s[i]) {
    putchar(s[i++]);
  }
  return 1;
}
