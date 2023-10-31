#include <stdlib.h>

#include <kernel/console.h>
#include <kernel/framebuffer.h>

void fb_putchar(char c);

int putchar(int ch) {
  if(fb_active()) {
    fb_putchar(ch);
    return 0;
  }
  return -1;
}

int puts(const char* text) {
  if(fb_active()) {
    while(*text) {
      putchar(*text++);
    }
    return 0;
  }
  return -1;
}

int vprintf(const char* format, va_list args) {
  for(int pos = 0; format[pos]; pos++) {
    if(format[pos] == '%') {
      switch(format[++pos]) {
        case '%': {
          putchar('%');
          break;
        }
        case 'b': {
          int val = va_arg(args, uint32_t);
          char str[256];
          utoa(val, str, 2);
          puts(str);
          break;
        }
        case 'c': {
          const int val = va_arg(args, int32_t);
          putchar(val);
          break;
        }
        case 'i':
        case 'd': {
          int val = va_arg(args, int32_t);
          char str[256];
          itoa(val, str, 10);
          puts(str);
          break;
        }
        case 'l': {
          switch(format[pos+1]) {
            case 'u': {
              uint64_t val = va_arg(args, uint64_t);
              char str[256];
              lutoa(val, str, 10);
              puts(str);
              pos++;
              break;
            }
            case 'l':
              int64_t val = va_arg(args, int64_t);
              char str[256];
              ltoa(val, str, 10);
              puts(str);
              pos++;
              break;
            case 'x':
              uint64_t val = va_arg(args, uint64_t);
              char str[256];
              lutoa(val, str, 16);
              puts(str);
              break;
            default: {
              int64_t val = va_arg(args, int64_t);
              char str[256];
              itoa(val, str, 10);
              puts(str);
              break;
            }
          }
          break;
        }
        case 'o': {
          int val = va_arg(args, uint32_t);
          char str[256];
          utoa(val, str, 8);
          puts(str);
          break;
        }
        case 's': {
          const char* val = (const char*)va_arg(args, char*);
          puts(val);
          break;
        }
        case 'u': {
          int val = va_arg(args, uint32_t);
          char str[256];
          utoa(val, str, 10);
          puts(str);
          break;
        }
        case 'X':
        case 'x': {
          uint64_t val = va_arg(args, uint64_t);
          char str[256];
          lutoa(val, str, 16);
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

int printf(const char* format, ...) {
  va_list list;
  va_start(list, format);
  int i = vprintf(format, list);
  va_end(list);
  return i;
}
