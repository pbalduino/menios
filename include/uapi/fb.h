#pragma once

#include <stdint.h>

#define FB_INFO_FLAG_DOUBLE_BUFFER   (1u << 0)
#define FB_INFO_FLAG_PALETTE_CONTROL (1u << 1)
#define FB_INFO_FLAG_SOFTWARE_SCALE  (1u << 2)

#define FB_FLIP_FLAG_PRESENT (1u << 0)

typedef struct fb_mode_info_t {
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t bpp;
  uint32_t flags;
  uint32_t reserved;
  uint64_t buffer_size;
} fb_mode_info_t;

