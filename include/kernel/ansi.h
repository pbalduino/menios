#ifndef MENIOS_INCLUDE_KERNEL_ANSI_H
#define MENIOS_INCLUDE_KERNEL_ANSI_H

#include <stddef.h>
#include <stdint.h>

#define ANSI_ATTR_BOLD       0x01
#define ANSI_ATTR_DIM        0x02
#define ANSI_ATTR_UNDERLINE  0x04
#define ANSI_ATTR_REVERSE    0x08

typedef struct ansi_style_t {
  uint8_t fg;
  uint8_t bg;
  uint8_t attrs;
} ansi_style_t;

void ansi_style_reset(ansi_style_t* style);
void ansi_style_apply_sgr(ansi_style_t* style, const int* params, size_t param_count);
void ansi_style_effective_colors(const ansi_style_t* style, uint32_t* fg, uint32_t* bg);
uint32_t ansi_palette_color(uint8_t index);
uint32_t ansi_dim_color(uint32_t rgb);

#endif
