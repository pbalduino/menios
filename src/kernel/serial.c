#include <stdio.h>
#include <stdlib.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>

// static FILE* com1 = NULL;

void serial_init() {
  printf("- Initing serial communication");
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
  printf(".OK\n");
}

int serial_putchar(int ch) {
  // Wait for the serial port to be ready
  while ((inb(0x3f8 + 5) & 0x20) == 0);

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

int svprintf(const char *format, va_list args){
  char str[256];

  for(int pos = 0; format[pos]; pos++) {
    if(format[pos] == '%') {
      switch(format[++pos]) {
        case '%': {
          serial_putchar('%');
          break;
        }
        case 'b': {
          int val = va_arg(args, uint32_t);
          utoa(val, str, 2);
          serial_puts(str);
          break;
        }
        case 'c': {
          const int val = va_arg(args, int32_t);
          serial_putchar(val);
          break;
        }
        case 'i':
        case 'd': {
          int val = va_arg(args, int32_t);
          itoa(val, str, 10);
          serial_puts(str);
          break;
        }
        case 'l': {
          switch(format[pos + 1]) {
            case 'u': {
              uint64_t val = va_arg(args, uint64_t);
              lutoa(val, str, 10);
              serial_puts(str);
              pos++;
              break;
            }
            case 'l':{
              int64_t val = va_arg(args, int64_t);
              ltoa(val, str, 10);
              serial_puts(str);
              pos++;
              break;
            }
            case 'x': {
              uint64_t val = va_arg(args, uint64_t);
              lutoa(val, str, 16);
              serial_puts("0x");
              serial_puts(str);
              pos++;
              break;
            }
            default: {
              int64_t val = va_arg(args, int64_t);
              itoa(val, str, 10);
              serial_puts(str);
              break;
            }
          }
          break;
        }
        case 'o': {
          int val = va_arg(args, uint32_t);
          utoa(val, str, 8);
          serial_puts(str);
          break;
        }
        case 'p': {
          void* val = va_arg(args, void*);
          lutoa((uintptr_t)val, str, 16);
          serial_printf("0x%s", str);
          break;
        }
        case 's': {
          const char* val = (const char*)va_arg(args, char*);
          serial_puts(val == NULL ? "<null>" : val);
          break;
        }
        case 'u': {
          uint32_t val = va_arg(args, uint32_t);
          utoa(val, str, 10);
          serial_puts(str);
          break;
        }
        case 'X':
        case 'x': {
          uint64_t val = va_arg(args, uint64_t);
          lutoa(val, str, 16);
          serial_puts(str);
          break;
        }
        default: {
         serial_putchar('?');
         serial_putchar(format[pos]);
         serial_putchar('?');
        }
      }
    } else {
      serial_putchar(format[pos]);
    }
  }
  return 0;
}

int serial_printf(const char* format, ...) {
  // printf("serial fd: %p", com1);

  va_list list;
  va_start(list, format);
  int i = svprintf(format, list);
  va_end(list);
  return i;
}
