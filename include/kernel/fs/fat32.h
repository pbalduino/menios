#ifndef MENIOS_INCLUDE_KERNEL_FS_FAT32_H
#define MENIOS_INCLUDE_KERNEL_FS_FAT32_H

#include <types.h>

#include <kernel/fs.h>

fs_driver_t* fs_fat32;

fs_driver_t* init_fat32(ata_t*);

#endif
