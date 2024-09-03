#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <kernel/console.h>
#include <kernel/file.h>
#include <kernel/serial.h>

int fputchar(int ch, FILE* file) {
  serial_log("Entering fputchar");
  if(file == NULL) {
    printf("file is null ");
    serial_error("file is null\n");
    return -1;
  }

  file_descriptor_t fd = fd_get(file->reserved);
  if(fd == NULL || !fd->used) {
    return -1;
  }

  serial_log("Leaving fputchar");
  return fd->write(ch);
}

int putchar(int ch) {
  return fputchar(ch, stdout);
}

int puts(const char* text) {
  return fputs(text, stdout);
}

int fputs(const char* text, FILE* file) {
  while(*text) {
    fputchar(*text++, file);
  }
  return 0;
}

int vprintf(const char* format, va_list args) {
  return fvprintf(stdout, format, args);
}

int fvprintf(FILE *file, const char *format, va_list args){
  for(int pos = 0; format[pos]; pos++) {
    if(format[pos] == '%') {
      switch(format[++pos]) {
        case '%': {
          fputchar('%', file);
          break;
        }
        case 'b': {
          int val = va_arg(args, uint32_t);
          char str[256];
          utoa(val, str, 2);
          fputs(str, file);
          break;
        }
        case 'c': {
          const int val = va_arg(args, int32_t);
          fputchar(val, file);
          break;
        }
        case 'i':
        case 'd': {
          int val = va_arg(args, int32_t);
          char str[256];
          itoa(val, str, 10);
          fputs(str, file);
          break;
        }
        case 'l': {
          switch(format[pos + 1]) {
            case 'u': {
              uint64_t val = va_arg(args, uint64_t);
              char str[256];
              lutoa(val, str, 10);
              fputs(str, file);
              pos++;
              break;
            }
            case 'l':{
              switch(format[pos + 2]) {
              case 'x': {
                  uint64_t val = va_arg(args, uint64_t);
                  char str[256];
                  ltoa(val, str, 16);
                  fputs(str, file);
                  pos++;
                  break;
                }
              default:{
                  int64_t val = va_arg(args, int64_t);
                  char str[256];
                  ltoa(val, str, 10);
                  fputs(str, file);
                  break;
                }
              }
              break;
            }
            case 'x': {
              uint64_t val = va_arg(args, uint64_t);
              char str[256];
              lutoa(val, str, 16);
              fputs("0x", file);
              fputs(str, file);
              pos++;
              break;
            }
            default: {
              int64_t val = va_arg(args, int64_t);
              char str[256];
              itoa(val, str, 10);
              fputs(str, file);
              break;
            }
          }
          break;
        }
        case 'o': {
          int val = va_arg(args, uint32_t);
          char str[256];
          utoa(val, str, 8);
          fputs(str, file);
          break;
        }
        case 'p': {
          void* val = va_arg(args, void*);
          char str[256];
          lutoa((uintptr_t)&val, str, 16);
          fprintf(file, "0x%s", str);
          break;
        }
        case 's': {
          const char* val = (const char*)va_arg(args, char*);
          fputs(val, file);
          break;
        }
        case 'u': {
          int val = va_arg(args, uint32_t);
          char str[256];
          utoa(val, str, 10);
          fputs(str, file);
          break;
        }
        case 'X':
        case 'x': {
          uint64_t val = va_arg(args, uint64_t);
          char str[256];
          lutoa(val, str, 16);
          fputs(str, file);
          break;
        }
        default: {
         fputchar('?', file);
         fputchar(format[pos], file);
         fputchar('?', file);
        }
      }
    } else {
      fputchar(format[pos], file);
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

int fprintf(FILE* file, const char* format, ...) {
  va_list list;
  va_start(list, format);
  int i = fvprintf(file, format, list);
  va_end(list);
  return i;
}
