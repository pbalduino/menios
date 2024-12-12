#include <stdio.h>
#include <stdlib.h>
#include <types.h>

#include <kernel/console.h>
#include <kernel/framebuffer.h>
#include <kernel/file.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>
#include <kernel/tsc.h>

kmutex_t fvprintf_mutex;

int fputchar(int ch, FILE* file) {
  if(file == NULL) {
    printf("file is null ");
    serial_error("file is null\n");
    return -1;
  }

  file_descriptor_t fd = fd_get(file->reserved);
  if(fd == NULL || !fd->used) {
    return -1;
  }

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
  char buffer[1024];
  int len = vsprintk(buffer, format, args);

  kmutex_lock(&fvprintf_mutex);
  fputs(buffer, file);
  kmutex_unlock(&fvprintf_mutex);

  return len;
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

void logk(const char* format, ...) {
  uint64_t time = ns_from_boot() / 1000;
  puts("[");
  set_foreground_color(FB_GREEN);
  printf("%3lu.%06lu", time / 1000000, time % 1000000);
  set_foreground_color(FB_LIGHT_WHITE);
  puts("] ");

  va_list list;
  va_start(list, format);
  vprintf(format, list);
  va_end(list);
}

void errk(const char* format, ...) {
  uint64_t time = ns_from_boot() / 1000;
  puts("[");
  set_foreground_color(FB_ORANGE);
  printf("%3lu.%06lu", time / 1000000, time % 1000000);
  set_foreground_color(FB_LIGHT_WHITE);
  puts("] ");

  va_list list;
  va_start(list, format);
  vprintf(format, list);
  va_end(list);
}