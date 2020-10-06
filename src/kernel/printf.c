#include <kernel/console.h>
#include <stdlib.h>

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
          uint32_t val = va_arg(args, uint32_t);
          char str[256];
          utoa(val, str, 16);
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

int printf (const char* format, ...) {
  va_list list;
  va_start (list, format);
  int i = vprintf(format, list);
  va_end(list);
  return i;
}
