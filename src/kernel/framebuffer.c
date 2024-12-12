#include <stddef.h>
#include <boot/limine.h>
#include <kernel/console.h>
#include <kernel/fonts.h>
#include <kernel/framebuffer.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <types.h>

#define ANSI_NONE        0;
#define ANSI_SET_COLOR   1;

static volatile struct limine_framebuffer_request framebuffer_request = {
  .id = LIMINE_FRAMEBUFFER_REQUEST,
  .revision = 0
};

static bool active;
static bool ansi_code;
static int ansi_command;
static int ansi_color;
static int char_line_height;
static int char_line_width;
static int current_col;
static int current_row;
static int viewpoint_x;
static int viewpoint_y;
static uint32_t color_bg = FB_BLACK;
static uint32_t color_fg = FB_LIGHT_WHITE;

static struct limine_framebuffer *framebuffer;

static FILE* fb_d;
#define ROWS 48
#define COLS 157

inline uint64_t fb_count() {
  return framebuffer_request.response->framebuffer_count;
}

inline uint16_t fb_bpp() {
  return framebuffer->bpp;
}

inline uint64_t fb_mode_count() {
  return framebuffer->mode_count;
}

void gotoxy(uint32_t x, uint32_t y) {
  current_col = x;
  current_row = y;
}

void get_cursor_pos(screen_pos_t* pos) {
  pos->x = current_col;
  pos->y = current_row;
}

void fb_init() {
  // Ensure we got a framebuffer.
  if(framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
    serial_error("Panic in framebuffer.c:46");
    hcf();
  }

  // Fetch the first framebuffer.
  framebuffer = framebuffer_request.response->framebuffers[0];

  active = true;

  viewpoint_x = framebuffer->width - 20;
  viewpoint_y = framebuffer->height - 20;
  current_row = 0;
  current_col = 0;
  char_line_width = 8; //viewpoint_x / COLS;
  char_line_height = 16; // viewpoint_y / ROWS;

  fb_d = freopen("/dev/fb/0", "w", stdout);

  if(fb_d == NULL) {
    serial_error("freopen(/dev/fb/0) -> NULL");
    hcf();
  }
}

inline bool fb_active() {
  return active;
}

void fb_draw() {
  // Note: we assume the framebuffer model is RGB with 32-bit pixels.
  for(size_t i = 0; i < 100; i++) {
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

void fb_scroll_up(int lines) {
  uint32_t *fb_ptr = framebuffer->address;
  uint32_t *fb_ptr2 = framebuffer->address;

  for(int y = 0; y < (framebuffer->height - (lines * char_line_height)); y++) {
    for(int x = 0; x < (framebuffer->width - 20); x++) {
      fb_ptr[y * (framebuffer->pitch / 4) + x] = fb_ptr2[(y + (lines * char_line_height)) * (framebuffer->pitch / 4) + x];
    }
  }

  for(int y = (framebuffer->height - (lines * char_line_height)); y < framebuffer->height; y++) {
    for(int x = 0; x < (framebuffer->width - 20); x++) {
      fb_putpixel(x, y, color_bg);
    }
  }
}

int fb_putchar(int c) {
  bool stay = false;

  switch(c) {
    case '\0': {
      ansi_code = false;
      return 0;
    }
    case '\b': {
      ansi_code = false;
      stay = true;
      if(current_col > 0) {
        current_col--;
        c = ' ';
      }
      break;
    }
    case '\x1b': {
      ansi_code = true;
      ansi_color = 0;
      return 0;
    }
    case '\n': {
      ansi_code = false;
      current_col = 0;
      current_row++;
      if(current_row >= ROWS) {
        current_row = ROWS - 1;
        fb_scroll_up(1);
      }
      return 0;
    }
  }

  if(ansi_code) {
    switch (c) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        ansi_color = (ansi_color * 10) + (c - '0');
        break;
      }
      case ';': {
        break;
      }
      case 'm':
        ansi_code = false;
        break;
      }
    return 0;
  }

  glypht_t glyph = font_glyph(c);

  int x = (current_col * char_line_width) + 10;
  int y = (current_row * char_line_height) + 10;

  // fb_drawline(x, y, fb_width(), y, 0xff0000);
  // fb_drawline(x, y, x, fb_height(), 0xff0000);

  for(int gx = 0; gx < 8; gx++) {
    for(int gy = 0; gy < 16; gy++) {
      fb_putpixel(x + gx, y + gy, color_bg);
      if((glyph.points[gy] >> (7 - gx)) & 0x01) {
        fb_putpixel(x + gx, y + gy, color_fg);
      }
    }
  }
  if(current_col < COLS){
    if(!stay) {
      current_col++;
    }
  } else {
    current_col = 0;
    current_row++;
    if(current_row >= ROWS) {
      current_row = ROWS - 1;
      fb_scroll_up(1);
    }
  }

  return 0;
}

inline uint64_t fb_width() {
  return framebuffer->width;
}

inline uint64_t fb_height() {
  return framebuffer->height;
}

void fb_list_modes() {
  for(uint64_t m = 0; m < framebuffer->mode_count; m++) {
    printf("  %s%lu: %s%lu x %s%lu x %d %s",
      m < 10 ? "0" : "",
      m,
      framebuffer->modes[m]->width < 1000 ? " " : "",
      framebuffer->modes[m]->width,
      framebuffer->modes[m]->height < 1000 ? " " : "",
      framebuffer->modes[m]->height,
      framebuffer->modes[m]->bpp,
      m == framebuffer->mode_count - 1 ? "" : "|");

    if((m + 1) % 4 == 0 || m == framebuffer->mode_count - 1) {
      putchar('\n');
    } else {
      putchar('|');
    }
  }
}

void set_foreground_color(uint32_t color) {
  color_fg = color;
}

void set_background_color(uint32_t color) {
  color_bg = color;
}
