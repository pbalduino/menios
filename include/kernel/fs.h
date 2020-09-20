#ifndef MENIOS_INCLUDE_KERNEL_FS_H
#define MENIOS_INCLUDE_KERNEL_FS_H

#include <types.h>
#include <kernel/ata.h>

typedef struct {
  char* description;
  ata_t* ata;
} fs_driver_t;

typedef struct {
} fs_t;

fs_t fs;

void init_fs(fs_driver_t*(*fs_f)(ata_t*), ata_t*(*ata_f)());

#endif
