#include <stdio.h>
#include <kernel/kernel.h>

static FILE* com1 = NULL;

void serial_init() {
  // Disable interrupts
  outb(0x3f8 + 1, 0x00);

  // Set the baud rate (for 115200 bps)
  outb(0x3f8 + 3, 0x80);  // Enable DLAB (Divisor Latch Access Bit)
  outb(0x3f8 + 0, 0x03);  // Set divisor low byte (115200 bps)
  outb(0x3f8 + 1, 0x00);  // Set divisor high byte

  // Disable DLAB, set data bits, stop bits, and parity
  outb(0x3f8 + 3, 0x03);  // 8 data bits, 1 stop bit, no parity

  // Enable FIFO (First In, First Out)
  outb(0x3f8 + 2, 0xc7);

  com1 = fopen("/dev/ttyS0", "w");

  // Enable interrupts (optional, if using interrupts)
  // outb(0x3f8 + 1, 0x01);
}

int serial_putchar(int ch) {
  // Wait for the serial port to be ready
  while ((inb(0x3f8 + 5) & 0x20) == 0);

  // Send the character to the serial port
  outb(0x3F8, ch & 0xff);

  return 0;
}

int serial_puts(const char* text) {
  if(com1 != NULL) {
    return fputs(text, com1);
  }
  return -1;
}

int serial_printf(const char* format, ...) {
  va_list list;
  va_start(list, format);
  int i = fvprintf(com1, format, list);
  va_end(list);
  return i;
}
