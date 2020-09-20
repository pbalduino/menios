#include <stdio.h>
#include <kernel/fs.h>

void init_fs(fs_driver_t*(*fs_f)(ata_t*), ata_t*(*ata_f)()) {
  ata_t* ata = (ata_f)();
  fs_driver_t* driver = (fs_f)(ata);

  printf("Filesystem driver is %s\n", driver->description);
}
