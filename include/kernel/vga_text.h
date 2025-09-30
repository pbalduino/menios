#ifndef MENIOS_INCLUDE_KERNEL_VGA_TEXT_H
#define MENIOS_INCLUDE_KERNEL_VGA_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include <kernel/char_device.h>

#ifdef __cplusplus
extern "C" {
#endif

void vga_text_init(void);
void vga_text_clear(void);
void vga_text_set_colors(uint8_t fg, uint8_t bg);
void vga_text_putc(char ch);
void vga_text_write(const char* data, size_t length);
char_device_t* vga_text_char_device(void);

#ifdef __cplusplus
}
#endif

#endif
