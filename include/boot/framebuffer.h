#ifndef _BOOT_FRAMEBUFFER_H
#define _BOOT_FRAMEBUFFER_H 1

#include <kernel/types.h>

#define FB_BLACK 0x0
#define FB_BLUE 0x0000ff
#define FB_DARK_BLUE 0x00007f
#define FB_DARK_GREEN 0x007f00
#define FB_GREEN 0x00ff00
#define FB_LIGHT_WHITE 0xffffff
#define FB_RED 0xff0000
#define FB_WHITE 0x7f7f7f

uint64_t fb_height();
uint64_t fb_width();
void fb_draw();
void fb_init();
void fb_putchar(char c);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb);
void fb_puts(char* text);

#endif
