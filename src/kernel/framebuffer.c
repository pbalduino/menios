#include <stddef.h>

#include <boot/limine.h>

#include <kernel/ansi.h>
#include <kernel/console.h>
#include <kernel/fonts.h>
#include <kernel/framebuffer.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>
#include <kernel/proc.h>
#include <kernel/pmm.h>
#ifdef MENIOS_KERNEL
#include <menios/stdio_internal.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

#define FB_MARGIN_X 10
#define FB_MARGIN_Y 10
#define MAX_COLS 200
#define MAX_VISIBLE_ROWS 80
#define SCROLLBACK_LINES 1024
#define CSI_MAX_PARAMS 8

typedef enum {
  ANSI_STATE_NORMAL = 0,
  ANSI_STATE_ESCAPE,
  ANSI_STATE_CSI
} ansi_parser_state_t;

typedef struct {
  char ch;
  ansi_style_t style;
} console_cell_t;

static volatile struct limine_framebuffer_request framebuffer_request = {
  .id = LIMINE_FRAMEBUFFER_REQUEST,
  .revision = 0
};

static bool active;
static uint32_t char_line_height;
static uint32_t char_line_width;
static uint32_t visible_cols;
static uint32_t visible_rows;
static uint64_t cursor_row;
static uint32_t cursor_col;
static uint64_t viewport_row;
static uint64_t total_rows;
static ansi_style_t default_style;
static ansi_style_t current_style;
static ansi_style_t saved_style;
static bool has_saved_cursor;
static uint64_t saved_cursor_row;
static uint32_t saved_cursor_col;
static struct limine_framebuffer *framebuffer;
static FILE* fb_d;

#define FB_STDOUT_FD 1
#define FB_STDERR_FD 2

static phys_addr_t framebuffer_phys = PHYS_ADDR_INVALID;
static size_t framebuffer_buffer_size = 0;
static size_t framebuffer_buffer_size_aligned = 0;
static phys_addr_t framebuffer_backbuffer_phys = PHYS_ADDR_INVALID;
static void* framebuffer_backbuffer_virt = NULL;
static size_t framebuffer_backbuffer_pages = 0;
static uint64_t framebuffer_view_width = 0;
static uint64_t framebuffer_view_height = 0;
static size_t framebuffer_view_pitch = 0;
static size_t framebuffer_bytes_per_pixel = 0;
static uint64_t framebuffer_boot_width = 0;
static uint64_t framebuffer_boot_height = 0;
static size_t framebuffer_boot_pitch = 0;
static uint16_t framebuffer_boot_bpp = 0;
static bool framebuffer_console_enabled = true;
static uint32_t framebuffer_owner_pid = (uint32_t)-1;

static size_t fb_align_up(size_t value) {
  if(value == 0) {
    return PAGE_SIZE;
  }
  size_t remainder = value % PAGE_SIZE;
  if(remainder == 0) {
    return value;
  }
  return value + (PAGE_SIZE - remainder);
}

static console_cell_t console_buffer[SCROLLBACK_LINES][MAX_COLS];
static uint64_t row_versions[SCROLLBACK_LINES];

static ansi_parser_state_t ansi_state;
static int csi_params[CSI_MAX_PARAMS];
static size_t csi_param_count;
static bool csi_param_active;
static bool csi_private_sequence;

static void clear_line_segment(uint64_t row, uint32_t start_col, uint32_t end_col, const ansi_style_t* style);
static void render_viewport(void);
static void fb_release_backbuffer(void) {
  if(framebuffer_backbuffer_phys != PHYS_ADDR_INVALID && framebuffer_backbuffer_pages > 0) {
    pmm_free_pages(framebuffer_backbuffer_phys, framebuffer_backbuffer_pages);
  }
  framebuffer_backbuffer_phys = PHYS_ADDR_INVALID;
  framebuffer_backbuffer_virt = NULL;
  framebuffer_backbuffer_pages = 0;
}

static void fb_console_set_enabled(bool enable) {
  framebuffer_console_enabled = enable;
}

bool fb_has_owner(void) {
  return framebuffer_owner_pid != (uint32_t)-1;
}

bool fb_is_owner(uint32_t pid) {
  return fb_has_owner() && framebuffer_owner_pid == pid;
}

bool fb_acquire_owner(uint32_t pid) {
  if(pid == (uint32_t)-1) {
    return false;
  }
  if(fb_has_owner() && framebuffer_owner_pid != pid) {
    return false;
  }
  if(!fb_has_owner()) {
    framebuffer_owner_pid = pid;
    fb_console_set_enabled(false);
  }
  return true;
}

static void fb_restore_boot_mode(void) {
  fb_set_mode(framebuffer_boot_width, framebuffer_boot_height, framebuffer_boot_bpp);
  framebuffer_owner_pid = (uint32_t)-1;
  fb_console_set_enabled(true);
  render_viewport();
}

bool fb_release_owner(uint32_t pid) {
  if(!fb_has_owner() || framebuffer_owner_pid != pid) {
    return false;
  }
  fb_restore_boot_mode();
  return true;
}

bool fb_is_boot_mode(uint64_t width, uint64_t height, uint16_t bpp) {
  if(bpp == 0) {
    bpp = framebuffer_boot_bpp;
  }
  return width == framebuffer_boot_width &&
         height == framebuffer_boot_height &&
         bpp == framebuffer_boot_bpp;
}

void fb_get_boot_mode(framebuffer_mode_info_t* out) {
  if(out == NULL) {
    return;
  }
  out->width = framebuffer_boot_width;
  out->height = framebuffer_boot_height;
  out->pitch = framebuffer_boot_pitch;
  out->bpp = framebuffer_boot_bpp;
  out->reserved = 0;
}

inline uint64_t fb_count() {
  return framebuffer_request.response->framebuffer_count;
}

inline uint16_t fb_bpp() {
  return framebuffer->bpp;
}

inline uint64_t fb_mode_count() {
  return framebuffer->mode_count;
}

static inline uint32_t row_index(uint64_t row) {
  return (uint32_t)(row % SCROLLBACK_LINES);
}

static inline console_cell_t* cell_at(uint64_t row, uint32_t col) {
  return &console_buffer[row_index(row)][col];
}

static void ensure_row(uint64_t row, const ansi_style_t* style) {
  uint32_t idx = row_index(row);
  if(row_versions[idx] == row) {
    return;
  }

  row_versions[idx] = row;

  ansi_style_t fill_style = style ? *style : default_style;
  for(uint32_t col = 0; col < visible_cols; col++) {
    console_buffer[idx][col].ch = ' ';
    console_buffer[idx][col].style = fill_style;
  }
}

static void copy_row(uint64_t dst_row, uint64_t src_row) {
  ensure_row(src_row, &default_style);
  ensure_row(dst_row, &default_style);

  uint32_t dst_idx = row_index(dst_row);
  uint32_t src_idx = row_index(src_row);

  memcpy(console_buffer[dst_idx], console_buffer[src_idx], visible_cols * sizeof(console_cell_t));
  row_versions[dst_idx] = dst_row;
}

static void scroll_region_up(uint32_t lines) {
  if(lines == 0) {
    return;
  }

  if(lines >= visible_rows) {
    for(uint32_t offset = 0; offset < visible_rows; offset++) {
      uint64_t row = viewport_row + offset;
      ensure_row(row, &default_style);
      clear_line_segment(row, 0, visible_cols - 1, &default_style);
    }
    render_viewport();
    return;
  }

  uint64_t base = viewport_row;
  for(uint32_t offset = 0; offset < visible_rows - lines; offset++) {
    copy_row(base + offset, base + offset + lines);
  }

  for(uint32_t offset = visible_rows - lines; offset < visible_rows; offset++) {
    uint64_t row = base + offset;
    ensure_row(row, &default_style);
    clear_line_segment(row, 0, visible_cols - 1, &default_style);
  }

  render_viewport();
}

static void scroll_region_down(uint32_t lines) {
  if(lines == 0) {
    return;
  }

  if(lines >= visible_rows) {
    for(uint32_t offset = 0; offset < visible_rows; offset++) {
      uint64_t row = viewport_row + offset;
      ensure_row(row, &default_style);
      clear_line_segment(row, 0, visible_cols - 1, &default_style);
    }
    render_viewport();
    return;
  }

  uint64_t base = viewport_row;
  for(int32_t offset = (int32_t)visible_rows - 1; offset >= (int32_t)lines; offset--) {
    copy_row(base + (uint32_t)offset, base + (uint32_t)(offset - lines));
  }

  for(uint32_t offset = 0; offset < lines && offset < visible_rows; offset++) {
    uint64_t row = base + offset;
    ensure_row(row, &default_style);
    clear_line_segment(row, 0, visible_cols - 1, &default_style);
  }

  render_viewport();
}

void gotoxy(uint32_t x, uint32_t y) {
  cursor_col = x < visible_cols ? x : (visible_cols - 1);
  cursor_row = viewport_row + (y < visible_rows ? y : (visible_rows - 1));
}

void get_cursor_pos(screen_pos_t* pos) {
  if(!pos) {
    return;
  }

  pos->x = (uint8_t)cursor_col;
  pos->y = (uint8_t)(cursor_row - viewport_row);
}

static void draw_cell(uint64_t row, uint32_t col) {
  if(row < viewport_row || (row - viewport_row) >= visible_rows) {
    return;
  }

  console_cell_t* cell = cell_at(row, col);
  uint32_t fg_color;
  uint32_t bg_color;

  ansi_style_effective_colors(&cell->style, &fg_color, &bg_color);

  uint32_t screen_row = (uint32_t)(row - viewport_row);
  int x = FB_MARGIN_X + (int)(col * char_line_width);
  int y = FB_MARGIN_Y + (int)(screen_row * char_line_height);

  glypht_t glyph = font_glyph(cell->ch ? cell->ch : ' ');

  for(uint32_t gx = 0; gx < char_line_width; gx++) {
    for(uint32_t gy = 0; gy < char_line_height; gy++) {
      fb_putpixel((uint32_t)(x + gx), (uint32_t)(y + gy), bg_color);
    }
  }

  for(uint32_t gx = 0; gx < 8; gx++) {
    for(uint32_t gy = 0; gy < 16 && gy < char_line_height; gy++) {
      if((glyph.points[gy] >> (7 - gx)) & 0x01) {
        fb_putpixel((uint32_t)(x + gx), (uint32_t)(y + gy), fg_color);
      }
    }
  }

  if(cell->style.attrs & ANSI_ATTR_UNDERLINE) {
    uint32_t underline_y = (uint32_t)(y + char_line_height - 1);
    for(uint32_t gx = 0; gx < char_line_width; gx++) {
      fb_putpixel((uint32_t)(x + gx), underline_y, fg_color);
    }
  }
}

static void draw_row(uint64_t row) {
  if(row < viewport_row || (row - viewport_row) >= visible_rows) {
    return;
  }

  for(uint32_t col = 0; col < visible_cols; col++) {
    draw_cell(row, col);
  }
}

static void render_viewport(void) {
  if(!framebuffer_console_enabled) {
    return;
  }
  for(uint32_t visible = 0; visible < visible_rows; visible++) {
    uint64_t row = viewport_row + visible;
    ensure_row(row, &default_style);
    draw_row(row);
  }
}

void framebuffer_init(void) {
  if(framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
    serial_error("Panic in framebuffer.c:framebuffer_init");
    halt();
  }

  framebuffer = framebuffer_request.response->framebuffers[0];
  active = true;

  framebuffer_phys = virtual_to_physical((virt_addr_t)framebuffer->address);
  framebuffer_bytes_per_pixel = framebuffer->bpp / 8;
  if(framebuffer_bytes_per_pixel == 0) {
    framebuffer_bytes_per_pixel = 4;
  }

  framebuffer_boot_width = framebuffer->width;
  framebuffer_boot_height = framebuffer->height;
  framebuffer_boot_pitch = framebuffer->pitch;
  framebuffer_boot_bpp = framebuffer->bpp;
  framebuffer_console_enabled = true;

  framebuffer_view_width = 0;
  framebuffer_view_height = 0;
  framebuffer_view_pitch = 0;
  framebuffer_buffer_size = 0;
  framebuffer_buffer_size_aligned = 0;
  framebuffer_backbuffer_phys = PHYS_ADDR_INVALID;
  framebuffer_backbuffer_virt = NULL;
  framebuffer_backbuffer_pages = 0;

  if(!fb_set_mode(framebuffer->width, framebuffer->height, framebuffer->bpp)) {
    framebuffer_view_width = framebuffer->width;
    framebuffer_view_height = framebuffer->height;
    framebuffer_view_pitch = framebuffer->pitch;
    framebuffer_buffer_size = framebuffer_view_pitch * framebuffer_view_height;
    framebuffer_buffer_size_aligned = fb_align_up(framebuffer_buffer_size);
  }

  char_line_width = 8;
  char_line_height = 16;

  visible_cols = (framebuffer->width > (FB_MARGIN_X * 2))
                   ? (uint32_t)((framebuffer->width - (FB_MARGIN_X * 2)) / char_line_width)
                   : 0;
  if(visible_cols > MAX_COLS) {
    visible_cols = MAX_COLS;
  }

  visible_rows = (framebuffer->height > (FB_MARGIN_Y * 2))
                   ? (uint32_t)((framebuffer->height - (FB_MARGIN_Y * 2)) / char_line_height)
                   : 0;
  if(visible_rows > MAX_VISIBLE_ROWS) {
    visible_rows = MAX_VISIBLE_ROWS;
  }

  if(visible_cols == 0 || visible_rows == 0) {
    serial_error("framebuffer: invalid console geometry");
    halt();
  }

  for(size_t i = 0; i < SCROLLBACK_LINES; i++) {
    row_versions[i] = UINT64_MAX;
  }

  ansi_style_reset(&current_style);
  default_style = current_style;
  saved_style = current_style;
  has_saved_cursor = false;

  cursor_row = 0;
  cursor_col = 0;
  viewport_row = 0;
  total_rows = visible_rows;

  for(uint32_t row = 0; row < visible_rows; row++) {
    ensure_row(row, &default_style);
  }

  render_viewport();

  fb_d = freopen("/dev/console", "w", stdout);
  if(fb_d == NULL) {
    file_t* fb_stdout_file = file_create_framebuffer_console_file();
    file_t* fb_stderr_file = file_create_framebuffer_console_file();
    if(fb_stdout_file == NULL || fb_stderr_file == NULL) {
      if(fb_stdout_file != NULL) {
        file_unref(fb_stdout_file);
      }
      if(fb_stderr_file != NULL) {
        file_unref(fb_stderr_file);
      }
      serial_error("framebuffer: failed to create console file");
      halt();
    }

    proc_file_close(&kernel_process_info, FB_STDOUT_FD);
    proc_file_close(&kernel_process_info, FB_STDERR_FD);

    if(proc_file_install_at(&kernel_process_info, FB_STDOUT_FD, fb_stdout_file, 0) < 0 ||
       proc_file_install_at(&kernel_process_info, FB_STDERR_FD, fb_stderr_file, 0) < 0) {
      file_unref(fb_stdout_file);
      file_unref(fb_stderr_file);
      serial_error("framebuffer: failed to install console streams");
      halt();
    }

    file_unref(fb_stdout_file);
    file_unref(fb_stderr_file);
    fb_d = stdout;
  }
}

inline bool fb_active() {
  return active;
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb) {
  uint32_t *fb_ptr = framebuffer->address;
  fb_ptr[y * (framebuffer->pitch / 4) + x] = rgb;
}

static uint64_t max_viewport_row(void) {
  if(total_rows <= visible_rows) {
    return 0;
  }
  return total_rows - visible_rows;
}

static void sync_viewport_to_cursor(void) {
  uint64_t max_view = max_viewport_row();
  if(cursor_row >= viewport_row + visible_rows) {
    viewport_row = cursor_row >= max_view ? max_view : cursor_row - visible_rows + 1;
    render_viewport();
  } else if(cursor_row < viewport_row) {
    viewport_row = cursor_row;
    render_viewport();
  }
}

static void clear_range(uint64_t start_row, uint32_t start_col, uint64_t end_row, uint32_t end_col, const ansi_style_t* style) {
  if(end_row < start_row) {
    return;
  }

  for(uint64_t row = start_row; row <= end_row; row++) {
    ensure_row(row, style);
    uint32_t begin_col = (row == start_row) ? start_col : 0;
    uint32_t finish_col = (row == end_row) ? end_col : (visible_cols - 1);
    console_cell_t* cells = cell_at(row, 0);
    for(uint32_t col = begin_col; col <= finish_col && col < visible_cols; col++) {
      cells[col].ch = ' ';
      cells[col].style = style ? *style : default_style;
    }
    draw_row(row);
  }
}

static void clear_line_segment(uint64_t row, uint32_t start_col, uint32_t end_col, const ansi_style_t* style) {
  ensure_row(row, style);
  console_cell_t* cells = cell_at(row, 0);
  ansi_style_t fill_style = style ? *style : default_style;

  for(uint32_t col = start_col; col <= end_col && col < visible_cols; col++) {
    cells[col].ch = ' ';
    cells[col].style = fill_style;
  }

  draw_row(row);
}

static void advance_line(void) {
  cursor_col = 0;
  cursor_row++;
  ensure_row(cursor_row, &current_style);
  clear_line_segment(cursor_row, 0, visible_cols - 1, &current_style);

  if(cursor_row + 1 > total_rows) {
    total_rows = cursor_row + 1;
  }

  sync_viewport_to_cursor();
}

static void carriage_return(void) {
  cursor_col = 0;
}

static void tab_forward(void) {
  uint32_t spaces = 8 - (cursor_col % 8);
  for(uint32_t i = 0; i < spaces; i++) {
    fb_putchar(' ');
  }
}

static void backspace(void) {
  if(cursor_col == 0) {
    return;
  }
  cursor_col--;
  ensure_row(cursor_row, &default_style);
  console_cell_t* cell = cell_at(cursor_row, cursor_col);
  cell->ch = ' ';
  draw_cell(cursor_row, cursor_col);
}

static void write_char(int c) {
  ensure_row(cursor_row, &current_style);
  console_cell_t* cell = cell_at(cursor_row, cursor_col);
  cell->ch = (char)c;
  cell->style = current_style;
  draw_cell(cursor_row, cursor_col);

  cursor_col++;
  if(cursor_col >= visible_cols) {
    advance_line();
  }
}

static void cursor_move_vertical(int delta) {
  if(delta == 0) {
    return;
  }

  if(delta > 0) {
    cursor_row += (uint64_t)delta;
  } else {
    uint64_t magnitude = (uint64_t)(-delta);
    if(cursor_row < magnitude) {
      cursor_row = 0;
    } else {
      cursor_row -= magnitude;
    }
  }

  if(cursor_row >= total_rows) {
    total_rows = cursor_row + 1;
  }

  ensure_row(cursor_row, &default_style);
  sync_viewport_to_cursor();
  draw_row(cursor_row);
}

static void cursor_move_horizontal(int delta) {
  int64_t new_col = (int64_t)cursor_col + delta;
  if(new_col < 0) {
    cursor_col = 0;
  } else if((uint32_t)new_col >= visible_cols) {
    cursor_col = visible_cols - 1;
  } else {
    cursor_col = (uint32_t)new_col;
  }
}

static void framebuffer_set_cursor(uint64_t row, uint32_t col) {
  cursor_row = row;
  cursor_col = col < visible_cols ? col : (visible_cols - 1);

  if(cursor_row >= total_rows) {
    total_rows = cursor_row + 1;
  }

  ensure_row(cursor_row, &default_style);
  sync_viewport_to_cursor();
  draw_row(cursor_row);
}

static void clear_screen(uint32_t mode) {
  switch(mode) {
    case 0: {
      clear_line_segment(cursor_row, cursor_col, visible_cols - 1, &current_style);
      if(cursor_row + 1 < total_rows) {
        clear_range(cursor_row + 1, 0, total_rows - 1, visible_cols - 1, &default_style);
      }
      break;
    }
    case 1: {
      if(cursor_row > 0) {
        clear_range(0, 0, cursor_row - 1, visible_cols - 1, &default_style);
      }
      clear_line_segment(cursor_row, 0, cursor_col, &default_style);
      break;
    }
    case 2:
    default: {
      cursor_row = 0;
      cursor_col = 0;
      viewport_row = 0;
      total_rows = visible_rows;
      for(uint32_t row = 0; row < visible_rows; row++) {
        ensure_row(row, &default_style);
        clear_line_segment(row, 0, visible_cols - 1, &default_style);
      }
      render_viewport();
      break;
    }
  }
}

static void clear_line(uint32_t mode) {
  switch(mode) {
    case 0:
      clear_line_segment(cursor_row, cursor_col, visible_cols - 1, &current_style);
      break;
    case 1:
      clear_line_segment(cursor_row, 0, cursor_col, &current_style);
      break;
    case 2:
    default:
      clear_line_segment(cursor_row, 0, visible_cols - 1, &current_style);
      break;
  }
}

static int get_csi_param(size_t index, int default_value) {
  if(index >= csi_param_count) {
    return default_value;
  }
  return csi_params[index];
}

static void handle_csi_command(char final_byte) {
  if(csi_private_sequence) {
    ansi_state = ANSI_STATE_NORMAL;
    return;
  }

  switch(final_byte) {
    case 'A': {
      int amount = get_csi_param(0, 1);
      cursor_move_vertical(-amount);
      break;
    }
    case 'B': {
      int amount = get_csi_param(0, 1);
      cursor_move_vertical(amount);
      break;
    }
    case 'C': {
      int amount = get_csi_param(0, 1);
      cursor_move_horizontal(amount);
      break;
    }
    case 'D': {
      int amount = get_csi_param(0, 1);
      cursor_move_horizontal(-amount);
      break;
    }
    case 'E': {
      int amount = get_csi_param(0, 1);
      cursor_move_vertical(amount);
      cursor_col = 0;
      break;
    }
    case 'F': {
      int amount = get_csi_param(0, 1);
      cursor_move_vertical(-amount);
      cursor_col = 0;
      break;
    }
    case 'G': {
      int column = get_csi_param(0, 1);
      if(column < 1) {
        column = 1;
      }
      cursor_move_horizontal(column - 1 - (int)cursor_col);
      break;
    }
    case 'H':
    case 'f': {
      int row = get_csi_param(0, 1);
      int col = get_csi_param(1, 1);
      if(row < 1) {
        row = 1;
      }
      if(col < 1) {
        col = 1;
      }
      framebuffer_set_cursor(viewport_row + (uint64_t)(row - 1), (uint32_t)(col - 1));
      break;
    }
    case 'J': {
      uint32_t mode = (uint32_t)get_csi_param(0, 0);
      clear_screen(mode);
      break;
    }
    case 'K': {
      uint32_t mode = (uint32_t)get_csi_param(0, 0);
      clear_line(mode);
      break;
    }
    case 'S': {
      int amount = get_csi_param(0, 1);
      if(amount < 0) {
        amount = 0;
      }
      scroll_region_up((uint32_t)amount);
      break;
    }
    case 'T': {
      int amount = get_csi_param(0, 1);
      if(amount < 0) {
        amount = 0;
      }
      scroll_region_down((uint32_t)amount);
      break;
    }
    case 'm': {
      ansi_style_apply_sgr(&current_style, csi_params, csi_param_count);
      break;
    }
    case 's': {
      saved_cursor_row = cursor_row;
      saved_cursor_col = cursor_col;
      saved_style = current_style;
      has_saved_cursor = true;
      break;
    }
    case 'u': {
      if(has_saved_cursor) {
        cursor_row = saved_cursor_row;
        cursor_col = saved_cursor_col;
        current_style = saved_style;
        sync_viewport_to_cursor();
      }
      break;
    }
    default:
      break;
  }
}

static void csi_start(void) {
  csi_param_count = 0;
  csi_param_active = false;
  csi_private_sequence = false;
}

static void csi_finish_param(void) {
  if(!csi_param_active) {
    csi_params[csi_param_count++] = 0;
  } else {
    csi_param_count++;
  }
  csi_param_active = false;
}

int fb_putchar(int c) {
  if(!active || !framebuffer_console_enabled) {
    return 0;
  }

  switch(ansi_state) {
    case ANSI_STATE_NORMAL: {
      switch(c) {
        case '\x1b':
          ansi_state = ANSI_STATE_ESCAPE;
          return 0;
        case '\n':
          advance_line();
          sync_viewport_to_cursor();
          return 0;
        case '\r':
          carriage_return();
          return 0;
        case '\t':
          tab_forward();
          return 0;
        case '\b':
          backspace();
          return 0;
        case '\0':
          return 0;
        default:
          if(c >= ' ') {
            write_char(c);
          }
          return 0;
      }
    }
    case ANSI_STATE_ESCAPE: {
      if(c == '[') {
        ansi_state = ANSI_STATE_CSI;
        csi_start();
        return 0;
      }
      ansi_state = ANSI_STATE_NORMAL;
      return 0;
    }
    case ANSI_STATE_CSI: {
      if(c == '?') {
        csi_private_sequence = true;
        return 0;
      }

      if(c >= '0' && c <= '9') {
        if(!csi_param_active) {
          if(csi_param_count < CSI_MAX_PARAMS) {
            csi_params[csi_param_count] = 0;
          }
          csi_param_active = true;
        }
        if(csi_param_count < CSI_MAX_PARAMS) {
          csi_params[csi_param_count] = (csi_params[csi_param_count] * 10) + (c - '0');
        }
        return 0;
      }

      if(c == ';') {
        if(csi_param_count < CSI_MAX_PARAMS) {
          csi_finish_param();
        }
        return 0;
      }

      if(csi_param_active && csi_param_count < CSI_MAX_PARAMS) {
        csi_finish_param();
      }

      handle_csi_command((char)c);
      ansi_state = ANSI_STATE_NORMAL;
      return 0;
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

uint64_t fb_pitch() {
  return framebuffer->pitch;
}

void fb_get_geometry(framebuffer_geometry_t* out) {
  if(out == NULL || framebuffer == NULL) {
    return;
  }
  out->width = framebuffer_view_width ? framebuffer_view_width : framebuffer->width;
  out->height = framebuffer_view_height ? framebuffer_view_height : framebuffer->height;
  out->pitch = framebuffer_view_pitch ? framebuffer_view_pitch : framebuffer->pitch;
  out->bpp = framebuffer->bpp;
  out->reserved = 0;
}

phys_addr_t fb_physical_address(void) {
  return framebuffer_phys;
}

phys_addr_t fb_backbuffer_physical(void) {
  if(!fb_backbuffer_available()) {
    return PHYS_ADDR_INVALID;
  }
  return framebuffer_backbuffer_phys;
}

void* fb_backbuffer_virtual(void) {
  return framebuffer_backbuffer_virt;
}

bool fb_backbuffer_available(void) {
  return framebuffer_backbuffer_phys != PHYS_ADDR_INVALID &&
         framebuffer_backbuffer_virt != NULL &&
         framebuffer_backbuffer_pages > 0;
}

size_t fb_buffer_size(void) {
  return framebuffer_buffer_size;
}

void fb_flush_backbuffer(void) {
  if(!fb_backbuffer_available() || framebuffer == NULL) {
    return;
  }

  uint8_t* dest = (uint8_t*)framebuffer->address;
  const uint8_t* src = (const uint8_t*)framebuffer_backbuffer_virt;
  size_t hw_pitch = framebuffer->pitch;
  size_t copy_pitch = framebuffer_view_pitch;
  uint64_t view_height = framebuffer_view_height;
  if(copy_pitch > hw_pitch) {
    copy_pitch = hw_pitch;
  }
  for(uint64_t y = 0; y < view_height; y++) {
    memcpy(dest + y * hw_pitch, src + y * framebuffer_view_pitch, copy_pitch);
    if(copy_pitch < hw_pitch) {
      memset(dest + y * hw_pitch + copy_pitch, 0, hw_pitch - copy_pitch);
    }
  }
  for(uint64_t y = view_height; y < framebuffer->height; y++) {
    memset(dest + y * hw_pitch, 0, hw_pitch);
  }
}

bool fb_set_mode(uint64_t width, uint64_t height, uint16_t bpp) {
  if(framebuffer == NULL) {
    return false;
  }
  if(width == 0 || height == 0) {
    return false;
  }
  if(width > framebuffer->width || height > framebuffer->height) {
    return false;
  }
  if(bpp == 0) {
    bpp = framebuffer->bpp;
  }
  if(bpp != framebuffer->bpp) {
    return false;
  }

  size_t new_pitch = width * framebuffer_bytes_per_pixel;
  bool same_as_boot = (width == framebuffer_boot_width) &&
                      (height == framebuffer_boot_height) &&
                      (bpp == framebuffer_boot_bpp);
  if(same_as_boot) {
    new_pitch = framebuffer_boot_pitch;
  }
  size_t new_size = new_pitch * height;
  size_t new_size_aligned = fb_align_up(new_size);

  phys_addr_t new_phys = PHYS_ADDR_INVALID;
  void* new_virt = NULL;
  size_t new_pages = 0;

  if(new_size_aligned > 0) {
    new_pages = new_size_aligned / PAGE_SIZE;
    if((new_size_aligned % PAGE_SIZE) != 0) {
      new_pages++;
    }
    if(new_pages > 0) {
      phys_addr_t candidate = pmm_alloc_pages(new_pages);
      if(candidate != 0) {
        void* virt = (void*)physical_to_virtual(candidate);
        if(virt != NULL) {
          memset(virt, 0, new_size_aligned);
          new_phys = candidate;
          new_virt = virt;
        } else {
          pmm_free_pages(candidate, new_pages);
        }
      }
    }
  }

  if(new_phys != PHYS_ADDR_INVALID) {
    fb_release_backbuffer();
    framebuffer_backbuffer_phys = new_phys;
    framebuffer_backbuffer_pages = new_pages;
    framebuffer_backbuffer_virt = new_virt;
  } else {
    fb_release_backbuffer();
  }

  framebuffer_view_width = width;
  framebuffer_view_height = height;
  framebuffer_view_pitch = new_pitch;
  framebuffer_buffer_size = new_size;
  framebuffer_buffer_size_aligned = new_size_aligned;
  fb_console_set_enabled(same_as_boot);

  return true;
}

uint64_t fb_mode_count_total(void) {
  uint64_t total = 1;
  if(framebuffer != NULL && framebuffer->mode_count > 0 && framebuffer->modes != NULL) {
    total += framebuffer->mode_count;
  }
  return total;
}

bool fb_mode_info(uint64_t index, framebuffer_mode_info_t* out) {
  if(out == NULL) {
    return false;
  }

  if(index == 0) {
    out->width = framebuffer_view_width ? framebuffer_view_width : framebuffer->width;
    out->height = framebuffer_view_height ? framebuffer_view_height : framebuffer->height;
    out->pitch = framebuffer_view_pitch ? framebuffer_view_pitch : framebuffer->pitch;
    out->bpp = framebuffer->bpp;
    out->reserved = 0;
    return true;
  }

  if(framebuffer == NULL || framebuffer->mode_count == 0 || framebuffer->modes == NULL) {
    return false;
  }

  uint64_t actual = index - 1;
  if(actual >= framebuffer->mode_count) {
    return false;
  }

  struct limine_video_mode* mode = framebuffer->modes[actual];
  if(mode == NULL) {
    return false;
  }

  out->width = mode->width;
  out->height = mode->height;
  out->pitch = mode->pitch;
  out->bpp = mode->bpp;
  out->reserved = 0;
  return true;
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
  switch(color) {
    case FB_BLACK: current_style.fg = 0; break;
    case FB_DARK_RED: current_style.fg = 1; break;
    case FB_DARK_GREEN: current_style.fg = 2; break;
    case FB_DARK_YELLOW: current_style.fg = 3; break;
    case FB_DARK_BLUE: current_style.fg = 4; break;
    case FB_DARK_MAGENTA: current_style.fg = 5; break;
    case FB_DARK_CYAN: current_style.fg = 6; break;
    case FB_WHITE: current_style.fg = 7; break;
    case FB_LIGHT_BLACK: current_style.fg = 8; break;
    case FB_RED: current_style.fg = 9; break;
    case FB_GREEN: current_style.fg = 10; break;
    case FB_YELLOW: current_style.fg = 11; break;
    case FB_BLUE: current_style.fg = 12; break;
    case FB_MAGENTA: current_style.fg = 13; break;
    case FB_CYAN: current_style.fg = 14; break;
    case FB_LIGHT_WHITE: current_style.fg = 15; break;
    case FB_ORANGE: current_style.fg = 11; break;
    default: current_style.fg = 7; break;
  }
  default_style.fg = current_style.fg;
}

void set_background_color(uint32_t color) {
  switch(color) {
    case FB_BLACK: current_style.bg = 0; break;
    case FB_DARK_RED: current_style.bg = 1; break;
    case FB_DARK_GREEN: current_style.bg = 2; break;
    case FB_DARK_YELLOW: current_style.bg = 3; break;
    case FB_DARK_BLUE: current_style.bg = 4; break;
    case FB_DARK_MAGENTA: current_style.bg = 5; break;
    case FB_DARK_CYAN: current_style.bg = 6; break;
    case FB_WHITE: current_style.bg = 7; break;
    case FB_LIGHT_BLACK: current_style.bg = 8; break;
    case FB_RED: current_style.bg = 9; break;
    case FB_GREEN: current_style.bg = 10; break;
    case FB_YELLOW: current_style.bg = 11; break;
    case FB_BLUE: current_style.bg = 12; break;
    case FB_MAGENTA: current_style.bg = 13; break;
    case FB_CYAN: current_style.bg = 14; break;
    case FB_LIGHT_WHITE: current_style.bg = 15; break;
    case FB_ORANGE: current_style.bg = 11; break;
    default: current_style.bg = 0; break;
  }
  default_style.bg = current_style.bg;
}

void fb_scroll_up(int lines) {
  if(lines <= 0) {
    return;
  }

  uint64_t max_view = max_viewport_row();
  uint64_t new_view = viewport_row + (uint64_t)lines;
  if(new_view > max_view) {
    new_view = max_view;
  }
  viewport_row = new_view;
  render_viewport();
}

void fb_draw() {
  render_viewport();
}
