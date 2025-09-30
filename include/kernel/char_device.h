#ifndef MENIOS_INCLUDE_KERNEL_CHAR_DEVICE_H
#define MENIOS_INCLUDE_KERNEL_CHAR_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <kernel/file.h>

#define CHAR_DEVICE_NAME_MAX 64

typedef struct char_device char_device_t;

typedef struct char_device_ops {
  int (*open)(char_device_t* device, uint32_t mode, file_t** out_file);
} char_device_ops_t;

struct char_device {
  char                  name[CHAR_DEVICE_NAME_MAX];
  uint32_t              supported_modes;
  const char_device_ops_t* ops;
  void*                 driver_ctx;
  char_device_t*        next;
};

void char_device_system_init(void);
bool char_device_register(char_device_t* device);
void char_device_unregister(char_device_t* device);
char_device_t* char_device_lookup(const char* name);
int char_device_open(const char* name, uint32_t mode, file_t** out_file);

#ifdef __cplusplus
}
#endif

#endif
