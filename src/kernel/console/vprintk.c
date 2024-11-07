#include <kernel/console.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PADDING_NONE    0
#define PADDING_SPACE   1
#define PADDING_ZERO    2
#define PADDING_INVALID 3

int vprintk(char *str, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int len = vsprintk(str, format, args);
  va_end(args);
  return len;
}

int vsprintk(char* str, const char* format, va_list args) {
  int result_len = 0;
  
  bool parsing = false;
  int pad_type = PADDING_NONE;
  int pad_len = 0;

  for(int pos = 0; format[pos]; pos++) {
    if(!parsing && format[pos] == '%') {
      parsing = true;
      continue;
    }

    if(parsing) {
      switch (format[pos]) {
        case '0': {
          if(pad_type == PADDING_NONE) {
            pad_type = PADDING_ZERO;
            break;
          }
        }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
          if(pad_type == PADDING_NONE) {
            pad_type = PADDING_SPACE;
          } else if(pad_type == PADDING_INVALID) {
            parsing = false;
            str[result_len++] = format[pos];
            break;
          }

          pad_len = pad_len * 10 + (format[pos] - '0');
          break;
        }
        case 'c': {
          const int val = va_arg(args, int32_t);
          if(pad_type == PADDING_NONE) {
            pad_type = PADDING_INVALID;
          }

          if(pad_type == PADDING_SPACE || pad_type == PADDING_ZERO) {
            for(int i = 0; i < pad_len - 1; i++) {
              str[result_len++] = pad_type == PADDING_SPACE ? ' ' : '0';
            }
            pad_type = PADDING_NONE;
            pad_len = 0;
          }

          str[result_len++] = val;
          break;
        }
        case 'd': {
          const int tmp_size = 10;
          char tmp[tmp_size];
          const int val = va_arg(args, int32_t);
          itoa(val, tmp, 10);

          if(pad_type == PADDING_SPACE || pad_type == PADDING_ZERO) {
            int padding = pad_len - strnlen(tmp, tmp_size);

            for(int i = 0; i < padding; i++) {
              str[result_len++] = pad_type == PADDING_SPACE ? ' ' : '0';
            }
            pad_type = PADDING_NONE;
            pad_len = 0;
          }

          for(int i = 0; tmp[i]; i++) {
            str[result_len++] = tmp[i];
          }

          break;
        }
        default:
          parsing = false;
          str[result_len++] = format[pos];
          break;
        }
    } else {
      parsing = false;
      str[result_len++] = format[pos];
    }
  }
  str[result_len] = '\0';

  return result_len;
}