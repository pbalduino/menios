#include <assert.h>
#include <stdio.h>
#include <types.h>
#include <x86.h>
#include <kernel/ata.h>
#include <kernel/fs.h>
#include <kernel/pci.h>
#include <kernel/storage.h>
#include <kernel/fs/fat32.h>

uint16_t ata_port;

void init_storage() {
  init_ata(&pci_devices);

  init_fs(&init_fat32, &init_ata);

  printf("\n");
}
