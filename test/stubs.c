#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

proc_info_p current;

void disable_interrupts() {}

void enable_interrupts() {}

void halt() {
  exit(1);
}

int kmutex_lock(kmutex_t* mutex) {
  (void)mutex;
  return 0;
}

bool kmutex_trylock(kmutex_t* mutex) {
  (void)mutex;
  return true;
}

int kmutex_unlock(kmutex_t* mutex) {
  (void)mutex;
  return 0;
}

void memzero(void* s, uint64_t n) {
	memset(s, 0, n);
}

void* memsetl(void* v, int64_t c, size_t n) {
  if(n == 0) {
    return v;
  }

  if(((uintptr_t)v % 8) == 0 && (n % 8) == 0) {
    uint64_t* dest = (uint64_t*)v;
    for(size_t i = 0; i < n; i++) {
      dest[i] = (uint64_t)c;
    }
    return v;
  }

  memset(v, (int)(uint8_t)c, n);
  return v;
}

static void vdiscard(const char* fmt, va_list args) {
  (void)fmt;
  (void)args;
}

void serial_printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vdiscard(fmt, args);
  va_end(args);
}

void serial_puts(const char* str) {
  (void)str;
}

void serial_error(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vdiscard(fmt, args);
  va_end(args);
}

void serial_line(const char* str) {
  (void)str;
}

void logk(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vdiscard(fmt, args);
  va_end(args);
}
