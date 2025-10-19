#ifndef _KERNEL_FRAMEBUFFER_H
#define _KERNEL_FRAMEBUFFER_H 1

#include <types.h>

typedef struct framebuffer_geometry {
  uint64_t width;
  uint64_t height;
  uint64_t pitch;
  uint16_t bpp;
  uint16_t reserved;
} framebuffer_geometry_t;

typedef struct framebuffer_mode_info {
  uint64_t width;
  uint64_t height;
  uint64_t pitch;
  uint16_t bpp;
  uint16_t reserved;
} framebuffer_mode_info_t;

#define FB_BLACK        0x000000
#define FB_DARK_RED     0x7f0000
#define FB_DARK_GREEN   0x007f00
#define FB_DARK_YELLOW  0x7f7f00
#define FB_DARK_BLUE    0x00007f
#define FB_DARK_MAGENTA 0x7f007f
#define FB_DARK_CYAN    0x007f7f
#define FB_WHITE        0xc0c0c0
#define FB_LIGHT_BLACK  0x7f7f7f
#define FB_RED          0xff0000
#define FB_GREEN        0x00ff00
#define FB_YELLOW       0xffff00
#define FB_BLUE         0x0000ff
#define FB_MAGENTA      0xff00ff
#define FB_CYAN         0x00ffff
#define FB_LIGHT_WHITE  0xffffff
#define FB_ORANGE       0xff7f00

typedef struct limine_video_mode** limine_video_mode_list_t;
typedef struct limine_video_mode* limine_video_mode_t;

bool fb_active();

uint16_t fb_bpp();
uint64_t fb_count();
uint64_t fb_height();
uint64_t fb_mode_count();
uint64_t fb_pitch();
uint64_t fb_width();
void fb_get_geometry(framebuffer_geometry_t* out);
phys_addr_t fb_physical_address(void);
phys_addr_t fb_backbuffer_physical(void);
void* fb_backbuffer_virtual(void);
bool fb_backbuffer_available(void);
size_t fb_buffer_size(void);
void fb_flush_backbuffer(void);
bool fb_set_mode(uint64_t width, uint64_t height, uint16_t bpp);
uint64_t fb_mode_count_total(void);
bool fb_mode_info(uint64_t index, framebuffer_mode_info_t* out);
bool fb_has_owner(void);
bool fb_is_owner(uint32_t pid);
bool fb_acquire_owner(uint32_t pid);
bool fb_release_owner(uint32_t pid);
bool fb_is_boot_mode(uint64_t width, uint64_t height, uint16_t bpp);
void fb_get_boot_mode(framebuffer_mode_info_t* out);

void fb_draw();
void fb_init();
void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb);
int fb_putchar(int c);
void fb_list_modes();

#endif
