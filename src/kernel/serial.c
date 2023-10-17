#include <kernel/kernel.h>

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

  // Enable interrupts (optional, if using interrupts)
  // outb(0x3f8 + 1, 0x01);
}

void serial_putchar(uint8_t ch) {
  // Wait for the serial port to be ready
  while ((inb(0x3f8 + 5) & 0x20) == 0);

  // Send the character to the serial port
  outb(0x3F8, ch);
}

int serial_puts(const char* text) {
  while(*text) {
    serial_putchar(*text++);
  }
  return 0;
}
