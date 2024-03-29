#include <stddef.h>
#include <stdint.h>
#include <kernel/fonts.h>

static font_t font_list = {
  .glyphs = {}
};

const glypht_t GL_NULL = {
  .value = 0x00,
  .points = { 0x00, 0x00, 0x66, 0x42, 0x00, 0x42, 0x42, 0x42, 0x00, 0x42, 0x42, 0x66, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_SPACE = {
  .value = ' ',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_EXCLAMATION = {
  .value = '!',
  .points = { 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_PERCENT = {
  .value = '*',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000001,
    0b11000011,
    0b00000110,
    0b00001100,
    0b00011000,
    0b00110000,
    0b01100000,
    0b11000110,
    0b10000110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_ASTERISK = {
  .value = '*',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01000100,
    0b01101100,
    0b00111000,
    0b11111110,
    0b00111000,
    0b01101100,
    0b01000100,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_PLUS = {
  .value = '+',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00011000,
    0b00011000,
    0b01111110,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_COMMA = {
  .value = ',',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000110,
    0b00000110,
    0b00001100,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_MINUS = {
  .value = '-',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111110,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_PERIOD = {
  .value = '.',
  .points = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_SLASH = {
  .value = '/',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000001,
    0b00000011,
    0b00000111,
    0b00001110,
    0b00011100,
    0b00111000,
    0b01110000,
    0b11100000,
    0b11000000,
    0b10000000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_ZERO = {
  .value = '0',
  .points = {0x00, 0x00, 0x3C, 0x42, 0x42, 0x46, 0x4A, 0x52, 0x62, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00, 0x00}
};

const glypht_t GL_ONE = {
  .value = '1',
  .points = { 0x00, 0x00, 0x08, 0x18, 0x28, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_TWO = {
  .value = '2',
  .points = { 0x00, 0x00, 0x3c, 0x42, 0x42, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_THREE = {
  .value = '3',
  .points = {0x00, 0x00, 0x3c, 0x42, 0x42, 0x02, 0x1c, 0x02, 0x02, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_FOUR = {
  .value = '4',
  .points = { 0x00, 0x00, 0x02, 0x06, 0x0a, 0x12, 0x22, 0x42, 0x7e, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_FIVE = {
  .value = '5',
  .points = { 0x00, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x7c, 0x02, 0x02, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_SIX = {
  .value = '6',
  .points = { 0x00, 0x00, 0x1c, 0x20, 0x40, 0x40, 0x7c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_SEVEN = {
  .value = '7',
  .points = { 0x00, 0x00, 0x7e, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_EIGHT = {
  .value = '8',
  .points = { 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_NINE = {
  .value = '9',
  .points = { 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x02, 0x04, 0x38, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_COLON = {
  .value = ':',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00000000,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_SEMICOLON = {
  .value = ';',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000110,
    0b00000110,
    0b00000000,
    0b00000000,
    0b00000110,
    0b00000110,
    0b00001100,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_CAPITAL_A = {
  .value = 'A',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00010000,
    0b00111000,
    0b01101100,
    0b11000110,
    0b11000110,
    0b11111110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_CAPITAL_B = {
  .value = 'B',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11111100,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11111100,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11111100,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_CAPITAL_C = {
  .value = 'C',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111100,
    0b11000110,
    0b11000010,
    0b11000000,
    0b11000000,
    0b11000000,
    0b11000000,
    0b11000010,
    0b11000110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_CAPITAL_D = {
  .value = 'D',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11111000,
    0b11001100,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11001100,
    0b11111000,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_CAPITAL_E = {
  .value = 'E',
  .points = { 0x00, 0x00, 0xFE, 0xC0, 0xC0, 0xC0, 0xF8, 0xC0, 0xC0, 0xC0, 0xC0, 0xFE, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_CAPITAL_F = {
  .value = 'f',
  .points = { 0x00, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_CAPITAL_G = {
  .value = 'C',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111100,
    0b11000110,
    0b11000010,
    0b11000000,
    0b11000000,
    0b11000000,
    0b11011110,
    0b11000110,
    0b11000110,
    0b01111010,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_CAPITAL_H = {
  .value = 'H',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11111110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_CAPITAL_I = {
  .value = 'I',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00111100,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_CAPITAL_J = {
  .value = 'J',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00011110,
    0b00001100,
    0b00001100,
    0b00001100,
    0b00001100,
    0b00001100,
    0b00001100,
    0b11001100,
    0b11001100,
    0b01111000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_CAPITAL_K = {
  .value = 'K',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11100110,
    0b01100110,
    0b01100110,
    0b01101100,
    0b01111000,
    0b01111000,
    0b01101100,
    0b01100110,
    0b01100110,
    0b11100110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_CAPITAL_L = {
  .value = 'M',
  .points = { 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_CAPITAL_M = {
  .value = 'M',
  .points = { 0x00, 0x00, 0x82, 0xC6, 0xAA, 0x92, 0x92, 0x82, 0x82, 0x82, 0x82, 0x82, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_CAPITAL_N = {
  .value = 'N',
  .points = { 0x00, 0x00, 0x42, 0x42, 0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_CAPITAL_O = {
  .value = 'O',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111100,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_CAPITAL_P = {
  .value = 'S',
  .points = { 0x00, 0x00, 0x7C, 0x42, 0x42, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_CAPITAL_S = {
  .value = 'S',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111100,
    0b11000110,
    0b11000000,
    0b01100000,
    0b00111000,
    0b00001100,
    0b00000110,
    0b00000110,
    0b11000110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_CAPITAL_T = {
  .value = 'T',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11111110,
    0b10011010,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_CAPITAL_U = {
  .value = 'V',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_CAPITAL_V = {
  .value = 'V',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b01101100,
    0b00111000,
    0b00010000,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_CAPITAL_W = {
  .value = 'W',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11010110,
    0b11111110,
    0b11111110,
    0b11101110,
    0b01000100,
    0b00000000,
    0b00000000,
    0b00000000,
  }
};

const glypht_t GL_BRACKET_LEFT = {
  .value = '[',
  .points = { 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_BACKSLASH = {
  .value = '\\',
  .points = { 0x00, 0x00, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_BRACKET_RIGHT = {
  .value = ']',
  .points = { 0x00, 0x00, 0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_SMALL_A = {
  .value = 'a',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111000,
    0b00001100,
    0b01111100,
    0b11001100,
    0b11011100,
    0b01110110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_B = {
  .value = 'b',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11100000,
    0b01100000,
    0b01100000,
    0b01100000,
    0b01111100,
    0b01100110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_C = {
  .value = 'c',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111100,
    0b11000110,
    0b11000000,
    0b11000000,
    0b11000110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_D = {
  .value = 'd',
  .points = {0x00, 0x00, 0x02, 0x02, 0x02, 0x3e, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e, 0x00, 0x00, 0x00, 0x00}
};

const glypht_t GL_SMALL_E = {
  .value = 'e',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111100,
    0b11000110,
    0b11111110,
    0b11000000,
    0b11000110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_F = {
  .value = 'f',
  .points = { 0x00, 0x00, 0x0e, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00 }
};

const glypht_t GL_SMALL_G = {
  .value = 'g',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01110110,
    0b11011100,
    0b11001100,
    0b11001100,
    0b11001100,
    0b01111100,
    0b00001100,
    0b11001100,
    0b01111000
  }
};

const glypht_t GL_SMALL_H = {
  .value = 'h',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11100000,
    0b01100000,
    0b01100000,
    0b01100000,
    0b01101100,
    0b01110110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b11100110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_I = {
  .value = 'i',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00111000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_J = {
  .value = 'j',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000110,
    0b00000110,
    0b00000000,
    0b00001110,
    0b00000110,
    0b00000110,
    0b00000110,
    0b00000110,
    0b00000110,
    0b11000110,
    0b11000110,
    0b01111000
  }
};

const glypht_t GL_SMALL_K = {
  .value = 'k',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11100000,
    0b01100000,
    0b01100000,
    0b01100110,
    0b01101100,
    0b01111000,
    0b01111000,
    0b01101100,
    0b01100110,
    0b11100110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_L = {
  .value = 'l',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00111000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_M = {
  .value = 'm',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11101100,
    0b11111110,
    0b11010110,
    0b11010110,
    0b11010110,
    0b11010110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_N = {
  .value = 'n',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11011100,
    0b01111110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_O = {
  .value = 'o',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111100,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_P = {
  .value = 'p',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11011100,
    0b01110110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b01111100,
    0b01100000,
    0b01100000,
    0b11110000
  }
};

const glypht_t GL_SMALL_Q = {
  .value = 'q',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01110110,
    0b11001110,
    0b11001100,
    0b11001100,
    0b11001100,
    0b01111100,
    0b00001100,
    0b00001100,
    0b00011110
  }
};

const glypht_t GL_SMALL_R = {
  .value = 'r',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11011100,
    0b01110110,
    0b01100110,
    0b01100000,
    0b01100000,
    0b11110000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_S = {
  .value = 's',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111100,
    0b11000110,
    0b01110000,
    0b00011100,
    0b11000110,
    0b01111100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_T = {
  .value = 't',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00010000,
    0b00110000,
    0b11111100,
    0b00110000,
    0b00110000,
    0b00110000,
    0b00110110,
    0b00011100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_U = {
  .value = 'u',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11001100,
    0b11001100,
    0b11001100,
    0b11001100,
    0b11001100,
    0b01110110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_V = {
  .value = 'v',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000110,
    0b11000110,
    0b11000110,
    0b01101100,
    0b00111000,
    0b00010000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_W = {
  .value = 'w',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000110,
    0b11000110,
    0b11010110,
    0b11111110,
    0b11101110,
    0b01000100,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_X = {
  .value = 'x',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000110,
    0b01101100,
    0b00111000,
    0b01101100,
    0b11000110,
    0b11000110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_SMALL_Y = {
  .value = 'y',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b11000110,
    0b01111110,
    0b00000110,
    0b00001100,
    0b11111000
  }
};

const glypht_t GL_SMALL_Z = {
  .value = 'z',
  .points = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11111110,
    0b01101100,
    0b00011000,
    0b01100000,
    0b11000110,
    0b11111110,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

const glypht_t GL_PIPE = {
  .value = '|',
  .points = { 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00 }
};

extern uint8_t font_terminus[];

void font_init() {
  for (int c = 0; c < 0x100; c++) {
    font_list.glyphs[c] = GL_NULL;
  }

  font_list.glyphs[' '] = GL_SPACE;
  font_list.glyphs['!'] = GL_EXCLAMATION;
  font_list.glyphs['%'] = GL_PERCENT;
  font_list.glyphs['*'] = GL_ASTERISK;
  font_list.glyphs['+'] = GL_PLUS;
  font_list.glyphs[','] = GL_COMMA;
  font_list.glyphs['-'] = GL_MINUS;
  font_list.glyphs['.'] = GL_PERIOD;
  font_list.glyphs['/'] = GL_SLASH;
  font_list.glyphs['0'] = GL_ZERO;
  font_list.glyphs['1'] = GL_ONE;
  font_list.glyphs['2'] = GL_TWO;
  font_list.glyphs['3'] = GL_THREE;
  font_list.glyphs['4'] = GL_FOUR;
  font_list.glyphs['5'] = GL_FIVE;
  font_list.glyphs['6'] = GL_SIX;
  font_list.glyphs['7'] = GL_SEVEN;
  font_list.glyphs['8'] = GL_EIGHT;
  font_list.glyphs['9'] = GL_NINE;
  font_list.glyphs[':'] = GL_COLON;
  font_list.glyphs[';'] = GL_SEMICOLON;
  font_list.glyphs['A'] = GL_CAPITAL_A;
  font_list.glyphs['B'] = GL_CAPITAL_B;
  font_list.glyphs['C'] = GL_CAPITAL_C;
  font_list.glyphs['D'] = GL_CAPITAL_D;
  font_list.glyphs['E'] = GL_CAPITAL_E;
  font_list.glyphs['F'] = GL_CAPITAL_F;
  font_list.glyphs['G'] = GL_CAPITAL_G;
  font_list.glyphs['L'] = GL_CAPITAL_L;
  font_list.glyphs['M'] = GL_CAPITAL_M;
  font_list.glyphs['N'] = GL_CAPITAL_N;
  font_list.glyphs['O'] = GL_CAPITAL_O;
  font_list.glyphs['H'] = GL_CAPITAL_H;
  font_list.glyphs['I'] = GL_CAPITAL_I;
  font_list.glyphs['J'] = GL_CAPITAL_J;
  font_list.glyphs['P'] = GL_CAPITAL_P;
  font_list.glyphs['K'] = GL_CAPITAL_K;
  font_list.glyphs['S'] = GL_CAPITAL_S;
  font_list.glyphs['T'] = GL_CAPITAL_T;
  font_list.glyphs['U'] = GL_CAPITAL_U;
  font_list.glyphs['V'] = GL_CAPITAL_V;
  font_list.glyphs['W'] = GL_CAPITAL_W;
  font_list.glyphs['['] = GL_BRACKET_LEFT;
  font_list.glyphs['\\'] = GL_BACKSLASH;
  font_list.glyphs[']'] = GL_BRACKET_RIGHT;
  font_list.glyphs['a'] = GL_SMALL_A;
  font_list.glyphs['b'] = GL_SMALL_B;
  font_list.glyphs['c'] = GL_SMALL_C;
  font_list.glyphs['d'] = GL_SMALL_D;
  font_list.glyphs['e'] = GL_SMALL_E;
  font_list.glyphs['f'] = GL_SMALL_F;
  font_list.glyphs['g'] = GL_SMALL_G;
  font_list.glyphs['h'] = GL_SMALL_H;
  font_list.glyphs['i'] = GL_SMALL_I;
  font_list.glyphs['j'] = GL_SMALL_J;
  font_list.glyphs['k'] = GL_SMALL_K;
  font_list.glyphs['l'] = GL_SMALL_L;
  font_list.glyphs['m'] = GL_SMALL_M;
  font_list.glyphs['n'] = GL_SMALL_N;
  font_list.glyphs['o'] = GL_SMALL_O;
  font_list.glyphs['p'] = GL_SMALL_P;
  font_list.glyphs['q'] = GL_SMALL_Q;
  font_list.glyphs['r'] = GL_SMALL_R;
  font_list.glyphs['s'] = GL_SMALL_S;
  font_list.glyphs['t'] = GL_SMALL_T;
  font_list.glyphs['u'] = GL_SMALL_U;
  font_list.glyphs['v'] = GL_SMALL_V;
  font_list.glyphs['w'] = GL_SMALL_W;
  font_list.glyphs['x'] = GL_SMALL_X;
  font_list.glyphs['y'] = GL_SMALL_Y;
  font_list.glyphs['z'] = GL_SMALL_Z;
  font_list.glyphs['|'] = GL_PIPE;
};

glypht_t font_glyph(uint8_t ascii) {
  return font_list.glyphs[ascii];
};
