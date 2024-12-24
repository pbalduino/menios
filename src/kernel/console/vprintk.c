#include <kernel/console.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PADDING_NONE      0
#define PADDING_SPACE     1
#define PADDING_ZERO      2
#define PADDING_PRECISION 3
#define PADDING_INVALID   9

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
      switch(format[pos]) {
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
          bool negative = false;
          const int tmp_size = 32;
          char tmp[tmp_size];
          int val = va_arg(args, int32_t);
          
          if(pad_type == PADDING_ZERO && val < 0) {
            negative = true;
            val *= -1;
          }

          itoa(val, tmp, 10);

          if(pad_type == PADDING_SPACE || pad_type == PADDING_ZERO) {
            int padding = pad_len - strnlen(tmp, tmp_size) - (negative ? 1 : 0);

            if(negative) {
              str[result_len++] = '-';
            }

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
        case 'l': {
          switch(format[pos + 1]) {
            case 'd': {
              bool negative = false;
              const int tmp_size = 32;
              char tmp[tmp_size];
              int64_t val = va_arg(args, int64_t);
              
              if(pad_type == PADDING_ZERO && val < 0) {
                negative = true;
                val *= -1;
              }

              ltoa(val, tmp, 10);

              if(pad_type == PADDING_SPACE || pad_type == PADDING_ZERO) {
                int padding = pad_len - strnlen(tmp, tmp_size) - (negative ? 1 : 0);

                if(negative) {
                  str[result_len++] = '-';
                }

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
            case 'x': {
              pos++;
              const int tmp_size = 64;
              char tmp[tmp_size];
              uint64_t val = va_arg(args, uint64_t);
              lutoa(val, tmp, 16);

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
          }
          break;
        }
        case 'p': {
          void* val = va_arg(args, void*);
          char* prefix = "0x";
          char _tmp[256];
          lutoa((uintptr_t)val, _tmp, 16);
          while(*prefix) {
            str[result_len++] = *prefix++;
          };

          char* tmp = _tmp;

          while(*tmp) {
            str[result_len++] = *tmp++;
          }

          break;
        }
        case 's': {
          const char* val = (const char*)va_arg(args, char*);
          uint32_t max_len = UINT32_MAX;
          if(pad_type == PADDING_PRECISION) {
            max_len = pad_len;
          }
          while(*val && max_len--) {
            str[result_len++] = *val++;
          }
          break;
        }
        case 'u': {
          const int tmp_size = 32;
          char tmp[tmp_size];
          const int val = va_arg(args, uint32_t);
          utoa(val, tmp, 10);

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
        case 'X': {
          const int tmp_size = 64;
          char tmp[tmp_size];
          uint64_t val = va_arg(args, uint64_t);
          lutoca(val, tmp, 16);

          for(int i = 0; tmp[i]; i++) {
            str[result_len++] = tmp[i];
          }

          break;
        }
        case 'x': {
          const int tmp_size = 64;
          char tmp[tmp_size];
          uint32_t val = va_arg(args, uint32_t);
          utoa(val, tmp, 16);

          for(int i = 0; tmp[i]; i++) {
            str[result_len++] = tmp[i];
          }

          break;
        }
        // case '.': {
        //   if(pad_type == PADDING_NONE) {
        //     pad_type = PADDING_PRECISION;
        //     break;
        //   }
        //   str[result_len++] = '.';
        //   break;
        // }
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