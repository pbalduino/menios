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

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_FB_H */
