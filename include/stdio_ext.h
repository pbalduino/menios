#ifndef MENIOS_INCLUDE_STDIO_EXT_H
#define MENIOS_INCLUDE_STDIO_EXT_H

#include <stdio.h>
#include <stddef.h>

#ifndef __THROW
#define __THROW
#endif

#define FSETLOCKING_BYCALLER 0
#define FSETLOCKING_INTERNAL 1
#define FSETLOCKING_QUERY 2

static inline size_t __fbufsize(FILE* fp) {
  (void)fp;
  return 0;
}

static inline size_t __fpending(FILE* fp) {
  (void)fp;
  return 0;
}

static inline int __flbf(FILE* fp) {
  (void)fp;
  return 0;
}

static inline void _flushlbf(void) {
}

static inline int __fsetlocking(FILE* fp, int type) {
  (void)fp;
  (void)type;
  return 0;
}

#endif
