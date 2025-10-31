#ifndef MENIOS_INCLUDE_KERNEL_FS_DEVFS_H
#define MENIOS_INCLUDE_KERNEL_FS_DEVFS_H

#ifdef __cplusplus
extern "C" {
#endif /* MENIOS_INCLUDE_KERNEL_FS_DEVFS_H */

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

struct file;
typedef struct file file_t;

typedef struct char_device char_device_t;

typedef int (*char_device_open_fn)(char_device_t* device, int flags, file_t** out_file);

struct char_device {
  const char*         name;
  uint32_t            access_mode;
  char_device_open_fn open;
  void*               driver_data;
  dev_t               dev;
  unsigned int        minor_count;
};

void char_device_system_initialize(void);
int char_device_register(char_device_t* device);
void char_device_unregister(char_device_t* device);

typedef void (*char_device_iter_fn)(const char_device_t* device, void* context);
void char_device_iterate(char_device_iter_fn fn, void* context);
char_device_t* char_device_lookup(dev_t dev);
void char_device_reserve_major(unsigned int major);

bool devfs_mount(void);

#ifdef __cplusplus
}
#endif

#endif
