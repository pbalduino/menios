#ifndef _KERNEL_FRAMEBUFFER_H
#define _KERNEL_FRAMEBUFFER_H 1

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FB_BLACK 0x0
#define FB_BLUE 0x0000ff
#define FB_DARK_BLUE 0x00007f
#define FB_DARK_GREEN 0x007f00
#define FB_GREEN 0x00ff00
#define FB_LIGHT_WHITE 0xffffff
#define FB_RED 0xff0000
#define FB_WHITE 0x7f7f7f

typedef struct limine_video_mode** limine_video_mode_list_t;
typedef struct limine_video_mode* limine_video_mode_t;

bool fb_active();

uint16_t fb_bpp();
uint64_t fb_count();
uint64_t fb_height();
uint64_t fb_mode_count();
uint64_t fb_width();

void fb_draw();
void fb_init();
void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb);
int fb_putchar(int c);
void fb_list_modes();

#ifdef __cplusplus
}
#endif

#endif
