#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/errno.h>

#include <kernel/char_device.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/vga_text.h>

#define VGA_TEXT_PHYS_ADDRESS 0x00000000000B8000ULL
#define VGA_TEXT_ROWS 25
#define VGA_TEXT_COLS 80
#define VGA_TEXT_SIZE (VGA_TEXT_ROWS * VGA_TEXT_COLS)

static volatile uint16_t* vga_buffer = NULL;
static size_t cursor_row = 0;
static size_t cursor_col = 0;
static uint8_t current_fg = 0x07; // light grey
static uint8_t current_bg = 0x00; // black
static bool vga_initialized = false;

static inline uint8_t color_attribute(void) {
  return (uint8_t)((current_bg << 4) | (current_fg & 0x0F));
}

static inline uint16_t make_entry(char ch) {
  return ((uint16_t)color_attribute() << 8) | (uint8_t)ch;
}

static void update_cursor(void) {
  if(!vga_initialized) {
    return;
  }
  uint16_t position = (uint16_t)(cursor_row * VGA_TEXT_COLS + cursor_col);
  outb(0x3D4, 0x0F);
  outb(0x3D5, (uint8_t)(position & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

static void vga_text_scroll(void) {
  if(!vga_initialized) {
    return;
  }
  const size_t last_row_start = (VGA_TEXT_ROWS - 1) * VGA_TEXT_COLS;
  memmove((void*)vga_buffer,
          (const void*)(vga_buffer + VGA_TEXT_COLS),
          (VGA_TEXT_ROWS - 1) * VGA_TEXT_COLS * sizeof(uint16_t));
  for(size_t col = 0; col < VGA_TEXT_COLS; col++) {
    vga_buffer[last_row_start + col] = make_entry(' ');
  }
  cursor_row = VGA_TEXT_ROWS - 1;
  cursor_col = 0;
}

void vga_text_clear(void) {
  if(!vga_initialized) {
    return;
  }
  for(size_t idx = 0; idx < VGA_TEXT_SIZE; idx++) {
    vga_buffer[idx] = make_entry(' ');
  }
  cursor_row = 0;
  cursor_col = 0;
  update_cursor();
}

void vga_text_set_colors(uint8_t fg, uint8_t bg) {
  current_fg = fg & 0x0F;
  current_bg = bg & 0x0F;
}

void vga_text_putc(char ch) {
  if(!vga_initialized) {
    return;
  }

  switch(ch) {
    case '\n':
      cursor_col = 0;
      cursor_row++;
      if(cursor_row >= VGA_TEXT_ROWS) {
        vga_text_scroll();
      }
      update_cursor();
      return;
    case '\r':
      cursor_col = 0;
      update_cursor();
      return;
    case '\b':
      if(cursor_col > 0) {
        cursor_col--;
      } else if(cursor_row > 0) {
        cursor_row--;
        cursor_col = VGA_TEXT_COLS - 1;
      }
      vga_buffer[cursor_row * VGA_TEXT_COLS + cursor_col] = make_entry(' ');
      update_cursor();
      return;
    case '\t': {
      size_t spaces = 4 - (cursor_col % 4);
      for(size_t i = 0; i < spaces; i++) {
        vga_text_putc(' ');
      }
      return;
    }
    default:
      break;
  }

  vga_buffer[cursor_row * VGA_TEXT_COLS + cursor_col] = make_entry(ch);
  cursor_col++;
  if(cursor_col >= VGA_TEXT_COLS) {
    cursor_col = 0;
    cursor_row++;
    if(cursor_row >= VGA_TEXT_ROWS) {
      vga_text_scroll();
    }
  }
  update_cursor();
}

void vga_text_write(const char* data, size_t length) {
  if(data == NULL) {
    return;
  }
  for(size_t idx = 0; idx < length; idx++) {
    vga_text_putc(data[idx]);
  }
}

void vga_text_init(void) {
  if(vga_initialized) {
    return;
  }

  vga_buffer = (uint16_t*)physical_to_virtual(VGA_TEXT_PHYS_ADDRESS);
  if(vga_buffer == NULL) {
    return;
  }

  cursor_row = 0;
  cursor_col = 0;
  current_fg = 0x07;
  current_bg = 0x00;
  vga_initialized = true;
  vga_text_clear();
}

static int64_t vga_text_write_file(file_t* file, const void* buffer, size_t length) {
  (void)file;
  if(buffer == NULL) {
    return -EINVAL;
  }
  vga_text_write((const char*)buffer, length);
  return (int64_t)length;
}

static const file_ops_t vga_text_file_ops = {
  .read = NULL,
  .write = vga_text_write_file,
  .close = NULL,
  .seek = NULL,
};

static int vga_text_char_open_cb(char_device_t* device, uint32_t mode, file_t** out_file) {
  (void)device;
  if((mode & FILE_MODE_WRITE) == 0) {
    return -EACCES;
  }
  file_t* file = file_create(&vga_text_file_ops, NULL, FILE_MODE_WRITE);
  if(file == NULL) {
    return -ENOMEM;
  }
  *out_file = file;
  return 0;
}

static const char_device_ops_t vga_text_char_ops = {
  .open = vga_text_char_open_cb,
};

static char_device_t vga_text_char_device_instance = {
  .name = "/dev/vga/0",
  .supported_modes = FILE_MODE_WRITE,
  .ops = &vga_text_char_ops,
  .driver_ctx = NULL,
  .next = NULL,
};

char_device_t* vga_text_char_device(void) {
  vga_text_init();
  return &vga_text_char_device_instance;
}
