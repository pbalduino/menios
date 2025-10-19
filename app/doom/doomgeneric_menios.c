#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include <menios/fb.h>
#include <menios/input.h>

void DG_Log(const char* fmt, ...) {
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if(len < 0) {
    return;
  }
  if(len >= (int)sizeof(buffer)) {
    len = (int)sizeof(buffer) - 1;
  }

  const char prefix[] = "[doom] ";
  const char newline = '\n';
  write(STDERR_FILENO, prefix, sizeof(prefix) - 1);
  write(STDERR_FILENO, buffer, (size_t)len);
  write(STDERR_FILENO, &newline, 1);
}

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
static menios_fb_mode_request_t fb_original_mode = {0, 0, 0, 0};
static int fb_mode_changed = 0;
static int fb_acquired = 0;

static void DG_Shutdown(void);

void DG_Init(void) {
  DG_Log("DG_Init: starting");
  menios_fb_info_t fb_info;
  memset(&fb_info, 0, sizeof(fb_info));

  fb_fd = open("/dev/fb0", O_RDWR);
  if(fb_fd < 0) {
    perror("open(/dev/fb0)");
    DG_Log("DG_Init: open(/dev/fb0) failed (errno=%d)", errno);
    fb_fd = -1;
    return;
  }
  DG_Log("DG_Init: framebuffer device opened (fd=%d)", fb_fd);

  if(ioctl(fb_fd, MENIOS_FB_IOCTL_GET_INFO, &fb_info) < 0) {
    perror("ioctl(MENIOS_FB_IOCTL_GET_INFO)");
    DG_Log("DG_Init: initial GET_INFO ioctl failed (errno=%d)", errno);
    close(fb_fd);
    fb_fd = -1;
    return;
  }
  DG_Log("DG_Init: framebuffer info width=%lu height=%lu bpp=%u pitch=%lu",
         (unsigned long)fb_info.width,
         (unsigned long)fb_info.height,
         fb_info.bpp,
         (unsigned long)fb_info.pitch);

  fb_original_mode.width = fb_info.width;
  fb_original_mode.height = fb_info.height;
  fb_original_mode.bpp = (uint16_t)fb_info.bpp;
  fb_original_mode.reserved = 0;
  fb_mode_changed = 0;
  fb_acquired = 0;

  if(ioctl(fb_fd, MENIOS_FB_IOCTL_ACQUIRE, NULL) != 0) {
    perror("ioctl(MENIOS_FB_IOCTL_ACQUIRE)");
    DG_Log("DG_Init: framebuffer acquire failed (errno=%d)", errno);
    close(fb_fd);
    fb_fd = -1;
    return;
  }
  fb_acquired = 1;
  DG_Log("DG_Init: framebuffer acquired");

  if(fb_info.width >= DOOMGENERIC_RESX &&
     fb_info.height >= DOOMGENERIC_RESY &&
     fb_info.bpp >= 24) {
    menios_fb_mode_request_t mode_req = {
      .width = DOOMGENERIC_RESX,
      .height = DOOMGENERIC_RESY,
      .bpp = (uint16_t)fb_info.bpp,
      .reserved = 0,
    };
    if(ioctl(fb_fd, MENIOS_FB_IOCTL_SET_MODE, &mode_req) == 0) {
      if(mode_req.width != fb_original_mode.width ||
         mode_req.height != fb_original_mode.height ||
         mode_req.bpp != fb_original_mode.bpp) {
        DG_Log("DG_Init: framebuffer mode changed to %ux%u %u bpp",
               mode_req.width,
               mode_req.height,
               mode_req.bpp);
        fb_mode_changed = 1;
      }
      if(ioctl(fb_fd, MENIOS_FB_IOCTL_GET_INFO, &fb_info) < 0) {
        perror("ioctl(MENIOS_FB_IOCTL_GET_INFO)");
        DG_Log("DG_Init: GET_INFO after mode change failed (errno=%d)", errno);
        close(fb_fd);
        fb_fd = -1;
        return;
      }
      DG_Log("DG_Init: framebuffer info refreshed width=%lu height=%lu bpp=%u pitch=%lu",
             (unsigned long)fb_info.width,
             (unsigned long)fb_info.height,
             fb_info.bpp,
             (unsigned long)fb_info.pitch);
    } else {
      DG_Log("DG_Init: framebuffer mode change request rejected (errno=%d)", errno);
    }
  }

  if(fb_info.bpp < 24 || fb_info.pitch == 0 || fb_info.width == 0 || fb_info.height == 0) {
    fprintf(stderr, "meniOS framebuffer: unsupported geometry (%lux%lu %u bpp)\n",
            (unsigned long)fb_info.width,
            (unsigned long)fb_info.height,
            fb_info.bpp);
    close(fb_fd);
    fb_fd = -1;
    DG_Log("DG_Init: unsupported framebuffer geometry");
    return;
  }

  fb_pitch_bytes = (size_t)fb_info.pitch;
  fb_bytes_per_pixel = fb_info.bpp / 8;
  fb_width_pixels = (size_t)fb_info.width;
  fb_height_pixels = (size_t)fb_info.height;
  fb_map_size = fb_pitch_bytes * fb_height_pixels;
  DG_Log("DG_Init: framebuffer pitch=%zu bytes_per_pixel=%zu width=%zu height=%zu map_size=%zu",
         fb_pitch_bytes,
         fb_bytes_per_pixel,
         fb_width_pixels,
         fb_height_pixels,
         fb_map_size);

  void* map = mmap(NULL, fb_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
  if(map == MAP_FAILED) {
    perror("mmap(/dev/fb0)");
    DG_Log("DG_Init: mmap(/dev/fb0) failed (errno=%d)", errno);
    close(fb_fd);
    fb_fd = -1;
    return;
  }

  fb_pixels = (uint8_t*)map;
  DG_Log("DG_Init: framebuffer mapped at %p", (void*)fb_pixels);

  atexit(DG_Shutdown);
  DG_Log("DG_Init: completed");
}

void DG_DrawFrame(void) {
  if(fb_pixels == NULL || fb_bytes_per_pixel < 4 || DG_ScreenBuffer == NULL) {
    static int warned = 0;
    if(!warned) {
      DG_Log("DG_DrawFrame: skipping frame (fb_pixels=%p bytes_per_pixel=%zu screen_buffer=%p)",
             (void*)fb_pixels,
             fb_bytes_per_pixel,
             (void*)DG_ScreenBuffer);
      warned = 1;
    }
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
  static int logged_first_frame = 0;
  if(!logged_first_frame) {
    DG_Log("DG_DrawFrame: starting blit copy_width=%zu copy_height=%zu row_bytes=%zu pitch=%zu",
           copy_width,
           copy_height,
           row_copy_bytes,
           fb_pitch_bytes);
    logged_first_frame = 1;
  }
  for(size_t y = 0; y < copy_height; y++) {
    uint8_t* dest = fb_pixels + y * fb_pitch_bytes;
    const uint8_t* src = src_base + y * DOOMGENERIC_RESX * sizeof(uint32_t);
    memcpy(dest, src, row_copy_bytes);
  }

  if(fb_fd >= 0) {
    int rc = ioctl(fb_fd, MENIOS_FB_IOCTL_FLUSH, NULL);
    if(rc != 0) {
      DG_Log("DG_DrawFrame: framebuffer flush failed (errno=%d)", errno);
    }
  }
}

void DG_Shutdown(void) {
  DG_Log("DG_Shutdown: invoked");
  if(fb_pixels != NULL && fb_map_size > 0) {
    munmap(fb_pixels, fb_map_size);
    DG_Log("DG_Shutdown: unmapped framebuffer at %p", (void*)fb_pixels);
    fb_pixels = NULL;
  }
  if(fb_fd >= 0 && fb_mode_changed) {
    ioctl(fb_fd, MENIOS_FB_IOCTL_SET_MODE, &fb_original_mode);
    DG_Log("DG_Shutdown: restored original framebuffer mode");
  }
  if(fb_fd >= 0 && fb_acquired) {
    ioctl(fb_fd, MENIOS_FB_IOCTL_RELEASE, NULL);
    DG_Log("DG_Shutdown: released framebuffer");
  }
  if(fb_fd >= 0) {
    close(fb_fd);
    DG_Log("DG_Shutdown: closed framebuffer fd=%d", fb_fd);
    fb_fd = -1;
  }
  fb_mode_changed = 0;
  fb_acquired = 0;
  DG_Log("DG_Shutdown: completed");
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
    DG_Log("DG_GetKey: invalid arguments pressed=%p key=%p", (void*)pressed, (void*)key);
    return 0;
  }

  menios_key_event_t event;
  for(;;) {
    if(menios_input_poll(&event) != 0) {
      if(errno == EAGAIN) {
        return 0;
      }
      DG_Log("DG_GetKey: menios_input_poll failed (errno=%d)", errno);
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
  DG_Log("main: meniOS Doom starting (argc=%d)", argc);
  for(int i = 0; i < argc; i++) {
    if(argv != NULL && argv[i] != NULL) {
      DG_Log("main: argv[%d]=\"%s\"", i, argv[i]);
    } else {
      DG_Log("main: argv[%d]=<null>", i);
    }
  }

  doomgeneric_Create(argc, argv);
  DG_Log("main: doomgeneric_Create returned, entering tick loop");

  for(;;) {
    doomgeneric_Tick();
  }

  return 0;
}
