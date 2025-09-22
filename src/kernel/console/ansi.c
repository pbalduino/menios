#include <kernel/ansi.h>

#include <stdint.h>

static const uint32_t ansi_palette[16] = {
  0x000000, // black
  0x800000, // red
  0x008000, // green
  0x808000, // yellow
  0x000080, // blue
  0x800080, // magenta
  0x008080, // cyan
  0xc0c0c0, // white
  0x808080, // bright black (grey)
  0xff0000, // bright red
  0x00ff00, // bright green
  0xffff00, // bright yellow
  0x0000ff, // bright blue
  0xff00ff, // bright magenta
  0x00ffff, // bright cyan
  0xffffff  // bright white
};

void ansi_style_reset(ansi_style_t* style) {
  if(style == NULL) {
    return;
  }

  style->fg = 7; // white
  style->bg = 0; // black
  style->attrs = 0;
}

static uint32_t scale_component(uint32_t value, uint32_t numerator, uint32_t denominator) {
  return (value * numerator) / denominator;
}

uint32_t ansi_dim_color(uint32_t rgb) {
  uint32_t r = (rgb >> 16) & 0xff;
  uint32_t g = (rgb >> 8) & 0xff;
  uint32_t b = rgb & 0xff;

  r = scale_component(r, 1, 2);
  g = scale_component(g, 1, 2);
  b = scale_component(b, 1, 2);

  return (r << 16) | (g << 8) | b;
}

uint32_t ansi_palette_color(uint8_t index) {
  return ansi_palette[index % 16];
}

void ansi_style_effective_colors(const ansi_style_t* style, uint32_t* fg, uint32_t* bg) {
  if(style == NULL) {
    return;
  }

  uint8_t fg_index = style->fg % 16;
  uint8_t bg_index = style->bg % 16;

  uint32_t fg_color = ansi_palette_color(fg_index);
  uint32_t bg_color = ansi_palette_color(bg_index);

  if(style->attrs & ANSI_ATTR_BOLD) {
    if(fg_index < 8) {
      fg_color = ansi_palette_color(fg_index + 8);
    }
  }

  if(style->attrs & ANSI_ATTR_DIM) {
    fg_color = ansi_dim_color(fg_color);
  }

  if(style->attrs & ANSI_ATTR_REVERSE) {
    uint32_t tmp = fg_color;
    fg_color = bg_color;
    bg_color = tmp;
  }

  if(fg != NULL) {
    *fg = fg_color;
  }

  if(bg != NULL) {
    *bg = bg_color;
  }
}

static void handle_reset(ansi_style_t* style) {
  ansi_style_reset(style);
}

static void handle_color(ansi_style_t* style, int param) {
  if(param >= 30 && param <= 37) {
    style->fg = (uint8_t)(param - 30);
  } else if(param == 39) {
    style->fg = 7;
  } else if(param >= 40 && param <= 47) {
    style->bg = (uint8_t)(param - 40);
  } else if(param == 49) {
    style->bg = 0;
  } else if(param >= 90 && param <= 97) {
    style->fg = (uint8_t)((param - 90) + 8);
  } else if(param >= 100 && param <= 107) {
    style->bg = (uint8_t)((param - 100) + 8);
  }
}

void ansi_style_apply_sgr(ansi_style_t* style, const int* params, size_t param_count) {
  if(style == NULL) {
    return;
  }

  if(param_count == 0) {
    handle_reset(style);
    return;
  }

  for(size_t i = 0; i < param_count; i++) {
    int param = params[i];

    switch(param) {
      case 0:
        handle_reset(style);
        break;
      case 1:
        style->attrs |= ANSI_ATTR_BOLD;
        style->attrs &= (uint8_t)~ANSI_ATTR_DIM;
        break;
      case 2:
        style->attrs |= ANSI_ATTR_DIM;
        style->attrs &= (uint8_t)~ANSI_ATTR_BOLD;
        break;
      case 4:
        style->attrs |= ANSI_ATTR_UNDERLINE;
        break;
      case 7:
        style->attrs |= ANSI_ATTR_REVERSE;
        break;
      case 22:
        style->attrs &= (uint8_t)~(ANSI_ATTR_BOLD | ANSI_ATTR_DIM);
        break;
      case 24:
        style->attrs &= (uint8_t)~ANSI_ATTR_UNDERLINE;
        break;
      case 27:
        style->attrs &= (uint8_t)~ANSI_ATTR_REVERSE;
        break;
      default:
        handle_color(style, param);
        break;
    }
  }
}
