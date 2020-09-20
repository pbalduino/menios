#include <kernel/fs/fat32.h>

fs_driver_t* driver;

fs_driver_t* init_fat32(ata_t* ata) {
  driver->description = "FAT32\0";
  driver->ata = ata;
  return driver;
}
