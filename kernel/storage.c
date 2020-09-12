#include <assert.h>
#include <stdio.h>
#include <types.h>
#include <x86.h>
#include <kernel/ata.h>
#include <kernel/pci.h>
#include <kernel/storage.h>

uint16_t ata_port;

static int detect_storage(ata_t* ata) {
  int p = 0;
  for(p = 0; p < PCI_MAX_DEVICES; p++) {
    if(pci_devices[p].class == 1 &&
       pci_devices[p].subclass == 1 &&
       (pci_devices[p].progif == 0x80 ||
         pci_devices[p].progif == 0x8a)) {

      if(pci_devices[p].bar0 == 0 || pci_devices[p].bar0 == 1) {
        ata->primary_channel = 0x1f0;
      }
      if(pci_devices[p].bar2 == 0 || pci_devices[p].bar2 == 1) {
        ata->secondary_channel = 0x170;
      }
      return 1;
    }
  }
  return 0;
}

void init_storage() {
  assert(is_pci_ready);

  assert(detect_storage(&ata));

  uint8_t sector_data[512];

  ata_read(0, &sector_data, 1);

  assert(sector_data[510] == 0x55 && sector_data[511] == 0xaa);

  printf("Primary ATA storage is ready\n\n");
}
