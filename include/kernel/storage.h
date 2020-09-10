#ifndef MENIOS_INCLUDE_KERNEL_STORAGE_H
#define MENIOS_INCLUDE_KERNEL_STORAGE_H

typedef struct {
  uint16_t primary_channel;
  uint16_t secondary_channel;
} ide_t;

void init_storage();

#endif
