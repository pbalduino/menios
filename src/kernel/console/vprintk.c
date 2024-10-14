#include <kernel/console.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vprintk(char *str, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int len = vsprintk(str, format, args);
  va_end(args);
  return len;
}

int vsprintk(char* str, const char* format, va_list args) {
  int len = 0;
  bool parsing = false;
  for(int pos = 0; format[pos]; pos++) {
    if(!parsing && format[pos] == '%') {
      parsing = true;
      continue;
    }

    if(parsing) {
      switch (format[pos]) {
        case 'c': {
          const int val = va_arg(args, int32_t);
          str[len++] = val;
          break;
        }
        case 'd': {
          char tmp[10];
          const int val = va_arg(args, int32_t);
          itoa(val, tmp, 10);

          for(int i = 0; tmp[i]; i++) {
            str[len++] = tmp[i];
          }

          break;
        }
        default:
          parsing = false;
          str[len++] = format[pos];
          break;
        }
    } else {
      parsing = false;
      str[len++] = format[pos];
    }
  }
  str[len] = '\0';

  return len;
}