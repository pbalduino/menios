#ifndef MENIOS_INCLUDE_FB_H
#define MENIOS_INCLUDE_FB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menios_fb_info {
  uint64_t width;
  uint64_t height;
  uint64_t pitch;
  uint16_t bpp;
  uint16_t reserved;
} menios_fb_info_t;

#define MENIOS_FB_IOCTL_GET_INFO 0x4d454e01u /* 'MEN\x01' */
#define MENIOS_FB_IOCTL_FLUSH    0x4d454e02u /* 'MEN\x02' */
#define MENIOS_FB_IOCTL_SET_MODE 0x4d454e03u /* 'MEN\x03' */
#define MENIOS_FB_IOCTL_ENUM_MODES 0x4d454e04u /* 'MEN\x04' */
#define MENIOS_FB_IOCTL_ACQUIRE  0x4d454e05u /* 'MEN\x05' */
#define MENIOS_FB_IOCTL_RELEASE  0x4d454e06u /* 'MEN\x06' */

typedef struct menios_fb_mode_request {
  uint64_t width;
  uint64_t height;
  uint16_t bpp;
  uint16_t reserved;
} menios_fb_mode_request_t;

typedef struct menios_fb_mode {
  uint64_t width;
  uint64_t height;
  uint64_t pitch;
  uint16_t bpp;
  uint16_t reserved;
} menios_fb_mode_t;

typedef struct menios_fb_modes_request {
  menios_fb_mode_t* modes;
  uint64_t capacity;
  uint64_t written;
} menios_fb_modes_request_t;

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_FB_H */
