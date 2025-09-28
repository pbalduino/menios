#include <stdio.h>
#include <stdlib.h>
#include <kernel/console.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>

// static FILE* com1 = NULL;
bool serial_debug = false;
static spinlock_t serial_printf_lock;

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

/*
  com1 = fopen("/dev/ttyS0", "w");

  if(com1 == NULL) {
    serial_putchar('Q');
  } else {
    serial_putchar('K');
    serial_putchar(com1->reserved);
  }
*/
  // Enable interrupts (optional, if using interrupts)
  // outb(0x3f8 + 1, 0x01);
  spinlock_init(&serial_printf_lock);
  serial_puts("- Initing serial communication\n");
  serial_puts(".OK\n");
}

int serial_putchar(int ch) {
  // Wait for the serial port to be ready
  while((inb(0x3f8 + 5) & 0x20) == 0);

  // Send the character to the serial port
  outb(0x3F8, ch & 0xff);

  return 0;
}

int serial_puts(const char* text) {
  while(*text) {
    serial_putchar(*text++);
  };
/*
  if(com1 == NULL) {
    printf("com1 is null\n");
    return -1;
  }
  // printf("com1 is not null\n");
  // return fputs(text, com1);
  */
 return 0;
}

int serial_vprintf(const char *format, va_list args){
  char buffer[1024];
  int len = vsprintk(buffer, format, args);

  uint64_t flags = spinlock_lock_irqsave(&serial_printf_lock);
  serial_puts(buffer);
  spinlock_unlock_irqrestore(&serial_printf_lock, flags);

  return len;
}

int serial_printf(const char* format, ...) {
  if(serial_debug) {
    va_list list;
    va_start(list, format);
    int i = serial_vprintf(format, list);
    va_end(list);
    return i;
  }
  
  return 0;
}
