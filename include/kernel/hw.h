#ifndef MENIOS_INCLUDE_KERNEL_HPET_H
#define MENIOS_INCLUDE_KERNEL_HPET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/driver.h>

typedef struct hardware_device_t hardware_device_t;
typedef hardware_device_t* hardware_device_p;

struct hardware_device_t {
  char path[128];
  char hid[16];
  driver_p driver;
  hardware_device_p next;
};

void hardware_init();
hardware_device_p hardware_devices(void);
void hardware_log_devices(void);

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_KERNEL_HPET_H
