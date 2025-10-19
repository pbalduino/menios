#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "doomgeneric.h"
#include <menios/fb.h>
#include <menios/input.h>

static const unsigned char scancode_ascii_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x10] = 'q', [0x11] = 'w', [0x12] = 'e',
    [0x13] = 'r', [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']', [0x1E] = 'a',
    [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
    [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'',
    [0x29] = '`', [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
    [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x37] = '*', [0x39] = ' ',
};

static unsigned char menios_convert_key(const menios_key_event_t* event) {
  if(event == NULL) {
    return 0;
  }

  if(event->extended) {
    switch(event->scancode) {
      case 0x48: return KEY_UPARROW;
      case 0x50: return KEY_DOWNARROW;
      case 0x4B: return KEY_LEFTARROW;
      case 0x4D: return KEY_RIGHTARROW;
      case 0x47: return KEY_HOME;
      case 0x4F: return KEY_END;
      case 0x49: return KEY_PGUP;
      case 0x51: return KEY_PGDN;
      case 0x52: return KEY_INS;
      case 0x53: return KEY_DEL;
      case 0x1D: return KEY_RCTRL;
      case 0x38: return KEY_RALT;
      default:
        break;
    }
  } else {
    switch(event->scancode) {
      case 0x01: return KEY_ESCAPE;
      case 0x0E: return KEY_BACKSPACE;
      case 0x0F: return KEY_TAB;
      case 0x1C: return KEY_ENTER;
      case 0x1D: return KEY_FIRE;
      case 0x2A:
      case 0x36: return KEY_RSHIFT;
      case 0x38: return KEY_LALT;
      case 0x39: return KEY_USE;
      case 0x3A: return KEY_CAPSLOCK;
      case 0x3B: return KEY_F1;
      case 0x3C: return KEY_F2;
      case 0x3D: return KEY_F3;
      case 0x3E: return KEY_F4;
      case 0x3F: return KEY_F5;
      case 0x40: return KEY_F6;
      case 0x41: return KEY_F7;
      case 0x42: return KEY_F8;
      case 0x43: return KEY_F9;
      case 0x44: return KEY_F10;
      case 0x57: return KEY_F11;
      case 0x58: return KEY_F12;
      default:
        break;
    }
  }

  unsigned char ascii = event->ascii;
  if(ascii == 0 && event->scancode < sizeof(scancode_ascii_map)) {
    ascii = scancode_ascii_map[event->scancode];
  }
  if(ascii != 0) {
    return (unsigned char)tolower(ascii);
  }

  return 0;
}

static uint8_t* fb_pixels = NULL;
static size_t fb_pitch_bytes = 0;
static size_t fb_bytes_per_pixel = 0;
static size_t fb_width_pixels = 0;
static size_t fb_height_pixels = 0;
static size_t fb_map_size = 0;
static int fb_fd = -1;

static void DG_Shutdown(void);

void DG_Init(void) {
  menios_fb_info_t fb_info;
  memset(&fb_info, 0, sizeof(fb_info));

  fb_fd = open("/dev/fb/0", O_RDWR);
  if(fb_fd < 0) {
    perror("open(/dev/fb/0)");
    fb_fd = -1;
    return;
  }

  if(ioctl(fb_fd, MENIOS_FB_IOCTL_GET_INFO, &fb_info) < 0) {
    perror("ioctl(MENIOS_FB_IOCTL_GET_INFO)");
    close(fb_fd);
    fb_fd = -1;
    return;
  }

  if(fb_info.bpp < 24 || fb_info.pitch == 0 || fb_info.width == 0 || fb_info.height == 0) {
    fprintf(stderr, "meniOS framebuffer: unsupported geometry (%lux%lu %u bpp)\n",
            (unsigned long)fb_info.width,
            (unsigned long)fb_info.height,
            fb_info.bpp);
    close(fb_fd);
    fb_fd = -1;
    return;
  }

  fb_pitch_bytes = (size_t)fb_info.pitch;
  fb_bytes_per_pixel = fb_info.bpp / 8;
  fb_width_pixels = (size_t)fb_info.width;
  fb_height_pixels = (size_t)fb_info.height;
  fb_map_size = fb_pitch_bytes * fb_height_pixels;

  void* map = mmap(NULL, fb_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
  if(map == MAP_FAILED) {
    perror("mmap(/dev/fb/0)");
    close(fb_fd);
    fb_fd = -1;
    return;
  }

  fb_pixels = (uint8_t*)map;

  atexit(DG_Shutdown);
}

void DG_DrawFrame(void) {
  if(fb_pixels == NULL || fb_bytes_per_pixel < 4 || DG_ScreenBuffer == NULL) {
    return;
  }

  size_t copy_height = DOOMGENERIC_RESY;
  if(copy_height > fb_height_pixels) {
    copy_height = fb_height_pixels;
  }

  size_t copy_width = DOOMGENERIC_RESX;
  if(copy_width > fb_width_pixels) {
    copy_width = fb_width_pixels;
  }

  size_t row_copy_bytes = copy_width * sizeof(uint32_t);
  const uint8_t* src_base = (const uint8_t*)DG_ScreenBuffer;
  for(size_t y = 0; y < copy_height; y++) {
    uint8_t* dest = fb_pixels + y * fb_pitch_bytes;
    const uint8_t* src = src_base + y * DOOMGENERIC_RESX * sizeof(uint32_t);
    memcpy(dest, src, row_copy_bytes);
  }

  if(fb_fd >= 0) {
    int rc = ioctl(fb_fd, MENIOS_FB_IOCTL_FLUSH, NULL);
    (void)rc;
  }
}

void DG_Shutdown(void) {
  if(fb_pixels != NULL && fb_map_size > 0) {
    munmap(fb_pixels, fb_map_size);
    fb_pixels = NULL;
  }
  if(fb_fd >= 0) {
    close(fb_fd);
    fb_fd = -1;
  }
}

void DG_SleepMs(uint32_t ms) {
  struct timespec req = {
      .tv_sec = (time_t)(ms / 1000U),
      .tv_nsec = (long)((ms % 1000U) * 1000000UL),
  };

  nanosleep(&req, NULL);
}

uint32_t DG_GetTicksMs(void) {
  struct timespec ts;
  if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }

  uint64_t total_ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
  return (uint32_t)(total_ms & UINT32_MAX);
}

int DG_GetKey(int* pressed, unsigned char* key) {
  if(pressed == NULL || key == NULL) {
    return 0;
  }

  menios_key_event_t event;
  for(;;) {
    if(menios_input_poll(&event) != 0) {
      if(errno == EAGAIN) {
        return 0;
      }
      return 0;
    }

    unsigned char doom_key = menios_convert_key(&event);
    if(doom_key == 0) {
      continue;
    }

    *pressed = event.pressed ? 1 : 0;
    *key = doom_key;
    return 1;
  }
}

void DG_SetWindowTitle(const char* title) {
  (void)title;
  // TODO: Propagate the window title to the meniOS window manager once it is implemented.
}

int main(int argc, char** argv) {
  doomgeneric_Create(argc, argv);

  for(;;) {
    doomgeneric_Tick();
  }

  return 0;
}
