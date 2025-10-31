#ifndef _KERNEL_DRIVER_H
#define _KERNEL_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

typedef struct driver_t {
  void (*start)(void);
  void (*shutdown)(void);
  uint8_t (*read)(void);
  void (*write)(void);
  int (*ioctl)(void* device, unsigned long request, void* argp);
  const char name[32];
  const char hid[12];
  void* device;
} driver_t;

typedef struct driver_t* driver_p;

typedef struct driver_list_t driver_list_t;
typedef struct driver_list_t* driver_list_p;

typedef struct driver_list_t {
    driver_p driver;
    driver_list_p next;
} driver_list_t;

typedef struct driver_list_t* driver_list_p;

void driver_registry_init(void);

void driver_register(driver_t*);
driver_p driver_load(const char* hid);

#ifdef __cplusplus
}
#endif

#endif /* _KERNEL_DRIVER_H */
