/* this loader should be as self-contained as possible, avoiding dependencies with Kernel */
#include <arch.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
#error nope

#define VGA_MEMORY 0xb8000
#define VGA_CTRL 0x3d4
#define VGA_DATA 0x3d5
#define VGA_HIGH_CURSOR 0x0f
#define VGA_LOW_CURSOR 0x0e
#define CRT_COLS 80
#define CRT_ROWS 25
#define CRT_SIZE (CRT_COLS * CRT_ROWS)

extern uint32_t BOOTLOADER_SECTORS;

uint16_t getpos() {
  uint16_t pos = 0;
  outb(VGA_CTRL, VGA_HIGH_CURSOR);
  io_wait();
  pos |= inb(VGA_DATA);
  outb(VGA_CTRL, VGA_LOW_CURSOR);
  io_wait();
  pos |= ((uint16_t)inb(VGA_DATA)) << 8;
  return pos;
}

void setpos(uint16_t pos) {
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
  uint16_t pos = getpos();

  if(ch == '\n') {
    pos -= (pos % CRT_COLS);
    pos += CRT_COLS;
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

  setpos(pos);

  return ch;
}

void lputs(const char *s) {
  while(*s) putchar(*s++);
}

void _panic(const char *file, int line, const char *fmt, ...) {
  if(fmt){ };
  char* str = "";
  itoa(line, str, 10);
  lputs("Kernel loader crashed @ ");
  lputs(file);
  lputs("(");
  lputs(str);
  lputs(")");
  while(true){ };
}

void load_kernel() {
  char* str = "";
  int sector = BOOTLOADER_SECTORS + 2;

  itoa(sector, str, 10);

  // print a welcome message - so we know it's working
  lputs("\nLoading kernel @ sector ");
  lputs(str);
  lputs("\n");
  // provide a very simple filesystem for boot - BFS maybe?
  // find the descriptor file
  // read it and retrieve the kernel file name, so we can have multiple kernels at same partition
  // find the kernel file
  // load the kernel file to memory
  // run the kernel
  lputs("Done\n");
}
