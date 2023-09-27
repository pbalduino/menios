#include <stddef.h>
#include <boot/limine.h>
#include <kernel/console.h>
#include <kernel/fonts.h>
#include <kernel/framebuffer.h>
#include <kernel/kernel.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
static volatile struct limine_framebuffer_request framebuffer_request = {
  .id = LIMINE_FRAMEBUFFER_REQUEST,
  .revision = 0
};

static bool active;
static int char_line_height;
static int char_line_width;
static int col;
static int row;
static int viewpoint_x;
static int viewpoint_y;
static struct limine_framebuffer *framebuffer;

#define ROWS 50
#define COLS 140

inline uint64_t fb_count() {
  return framebuffer_request.response->framebuffer_count;
}

inline uint16_t fb_bpp() {
  return framebuffer->bpp;
}

inline uint64_t fb_mode_count() {
  return framebuffer->mode_count;
}

void fb_init() {
  // Ensure we got a framebuffer.
  if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
    hcf();
  }

  // Fetch the first framebuffer.
  framebuffer = framebuffer_request.response->framebuffers[0];

  active = true;

  viewpoint_x = framebuffer->width - 20;
  viewpoint_y = framebuffer->height - 20;
  row = 0;
  col = 0;
  char_line_width = viewpoint_x / COLS;
  char_line_height = viewpoint_y / ROWS;
}

inline bool fb_active() {
  return active;
}

void fb_draw() {
  // Note: we assume the framebuffer model is RGB with 32-bit pixels.
  for (size_t i = 0; i < 100; i++) {
    uint32_t *fb_ptr = framebuffer->address;
    fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
  }
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb) {
  uint32_t *fb_ptr = framebuffer->address;
  fb_ptr[y * (framebuffer->pitch / 4) + x] = rgb;
}

// FIXME: use doublebuffer to draw the line
// TODO: need to implement malloc for this
void fb_drawline(uint32_t left, uint32_t right, uint32_t top, uint32_t bottom, uint32_t rgb) {
  for(uint32_t x = left; x < right; x++) {
    fb_putpixel(x, top, rgb);
  }
}

void fb_putchar(char c) {
  if(c == '\0') {
    return;
  }

  if(c == '\n') {
    col = 0;
    row++;
    return;
  }

  glypht_t glyph = font_glyph(c);

  int x = (col * char_line_width) + 10;
  int y = (row * char_line_height) + 10;

  // fb_drawline(x, y, fb_width(), y, 0xff0000);
  // fb_drawline(x, y, x, fb_height(), 0xff0000);

  for(int gx = 0; gx < 8; gx++) {
    for(int gy = 0; gy < 16; gy++) {
      if((glyph.points[gy] >> (7 - gx)) & 0x01) {
        fb_putpixel(x + gx, y + gy, FB_WHITE);
      }
    }
  }
  if(col < COLS){
    col++;
  } else {
    col = 0;
    row++;
  }
}

inline uint64_t fb_width() {
  return framebuffer->width;
}

inline uint64_t fb_height() {
  return framebuffer->height;
}

void fb_list_modes() {
  for(uint64_t m = 0; m < framebuffer->mode_count; m++) {
    printf("  %lu: %lu x %lu x %d | ", m, framebuffer->modes[m]->width, framebuffer->modes[m]->height, framebuffer->modes[m]->bpp);
    if(m > 0 && m % 4 == 0) {
      putchar('\n');
    }
  }
}
