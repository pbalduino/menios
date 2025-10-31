#include <kernel/framebuffer.h>
#include <stdbool.h>
#include <stddef.h>
#include <types.h>

#ifndef MENIOS_HOST_TEST
#error "stubs_framebuffer.c should only be built for host tests"
#endif

static framebuffer_geometry_t g_stub_geometry = {
  .width = 640,
  .height = 480,
  .pitch = 640 * 4,
  .bpp = 32,
  .reserved = 0,
};

static framebuffer_mode_info_t g_stub_boot_mode = {
  .width = 640,
  .height = 480,
  .pitch = 640 * 4,
  .bpp = 32,
  .reserved = 0,
};

bool fb_active(void) {
  return false;
}

uint16_t fb_bpp(void) {
  return g_stub_geometry.bpp;
}

uint64_t fb_count(void) {
  return 1;
}

uint64_t fb_height(void) {
  return g_stub_geometry.height;
}

uint64_t fb_mode_count(void) {
  return 1;
}

uint64_t fb_pitch(void) {
  return g_stub_geometry.pitch;
}

uint64_t fb_width(void) {
  return g_stub_geometry.width;
}

void fb_get_geometry(framebuffer_geometry_t* out) {
  if(out == NULL) {
    return;
  }
  *out = g_stub_geometry;
}

phys_addr_t fb_physical_address(void) {
  return 0;
}

phys_addr_t fb_backbuffer_physical(void) {
  return 0;
}

void* fb_backbuffer_virtual(void) {
  return NULL;
}

bool fb_backbuffer_available(void) {
  return false;
}

size_t fb_buffer_size(void) {
  return 0;
}

void fb_flush_backbuffer(void) {
}

bool fb_set_mode(uint64_t width, uint64_t height, uint16_t bpp) {
  (void)width;
  (void)height;
  (void)bpp;
  return false;
}

uint64_t fb_mode_count_total(void) {
  return 1;
}

bool fb_mode_info(uint64_t index, framebuffer_mode_info_t* out) {
  if(index != 0 || out == NULL) {
    return false;
  }
  *out = g_stub_boot_mode;
  return true;
}

bool fb_has_owner(void) {
  return false;
}

bool fb_is_owner(uint32_t pid) {
  (void)pid;
  return false;
}

bool fb_acquire_owner(uint32_t pid) {
  (void)pid;
  return true;
}

bool fb_release_owner(uint32_t pid) {
  (void)pid;
  return true;
}

bool fb_is_boot_mode(uint64_t width, uint64_t height, uint16_t bpp) {
  return width == g_stub_boot_mode.width &&
         height == g_stub_boot_mode.height &&
         bpp == g_stub_boot_mode.bpp;
}

void fb_get_boot_mode(framebuffer_mode_info_t* out) {
  if(out == NULL) {
    return;
  }
  *out = g_stub_boot_mode;
}

void fb_draw(void) {}
void framebuffer_initialize(void) {}
void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb) {
  (void)x;
  (void)y;
  (void)rgb;
}

/* Provided by test/stubs.c */
int fb_putchar(int c);

void fb_list_modes(void) {}
