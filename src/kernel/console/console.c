#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

#include <kernel/console.h>
#include <kernel/framebuffer.h>
#include <kernel/file.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/serial.h>
#include <kernel/tsc.h>

static spinlock_t fvprintf_lock;

int fputchar(int ch, FILE* file) {
  if(file == NULL) {
    return -EINVAL;
  }

  struct proc_info_t* proc = current ? current : &kernel_process_info;
  file_t* handle = proc_file_get(proc, file->reserved, NULL);
  if(handle == NULL) {
    return -EBADF;
  }

  char c = (char)ch;
  int64_t written = file_write(handle, &c, 1);
  file_unref(handle);
  return (int)written;
}

int putchar(int ch) {
  return fputchar(ch, stdout);
}

int kputchar(int ch) {
  return putchar(ch);
}

int puts(const char* text) {
  return fputs(text, stdout);
}

int fputs(const char* text, FILE* file) {
  if(file == NULL || text == NULL) {
    return -EINVAL;
  }

  struct proc_info_t* proc = current ? current : &kernel_process_info;
  file_t* handle = proc_file_get(proc, file->reserved, NULL);
  if(handle == NULL) {
    return -EBADF;
  }

  size_t len = strlen(text);
  int64_t written = file_write(handle, text, len);
  file_unref(handle);
  return (int)written;
}

int vprintf(const char* format, va_list args) {
  return fvprintf(stdout, format, args);
}

int fvprintf(FILE *file, const char *format, va_list args){
  char buffer[1024];
  int len = vsprintk(buffer, format, args);

  spinlock_lock(&fvprintf_lock);
  fputs(buffer, file);
  spinlock_unlock(&fvprintf_lock);

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
