#ifndef MENIOS_INCLUDE_KERNEL_NULL_DEVICE_H
#define MENIOS_INCLUDE_KERNEL_NULL_DEVICE_H

#include <kernel/char_device.h>

#ifdef __cplusplus
extern "C" {
#endif

char_device_t* null_char_device(void);

#ifdef __cplusplus
}
#endif

#endif
