#ifndef _KERNEL_FONTS_H
#define _KERNEL_FONTS_H 1

#include <types.h>

typedef struct {
  char value;
  uint8_t points[16];
} glypht_t;

typedef struct {
  glypht_t glyphs[0x100];
} font_t;

void font_init();
glypht_t font_glyph(uint8_t ascii);

#endif
