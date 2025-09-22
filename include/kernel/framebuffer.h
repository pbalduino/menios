#ifndef _KERNEL_FRAMEBUFFER_H
#define _KERNEL_FRAMEBUFFER_H 1

#include <types.h>

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
uint64_t fb_width();

void fb_draw();
void fb_init();
void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb);
int fb_putchar(int c);
void fb_list_modes();

#endif
