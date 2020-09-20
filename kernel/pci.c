#include <assert.h>
#include <types.h>
#include <stdio.h>
#include <x86.h>
#include <kernel/pci.h>

#define PCI_ENABLE_BIT     0x80000000
#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA    0xcfc

uint16_t vendor_id(uint32_t data) {
  return (uint16_t)(data & 0xffff);
}

uint16_t device_id(uint32_t data) {
  return (uint16_t)((data >> 16) & 0xffff);
}

uint32_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t lbus  = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;
  uint32_t address = PCI_ENABLE_BIT | (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | offset);

  outl(PCI_CONFIG_ADDRESS, address);

  return inl(PCI_CONFIG_DATA);
}

void init_pci() {
  assert(!is_pci_ready);

  printf("Detecting PCI devices... ");

  int count = 0;

  for(int bus = 0; bus < 0x100; bus++) {
    for(int slot = 0; slot < 0x20; slot++) {
      for(int func = 0; func < 8; func++) {
        uint32_t data = pci_read_word(bus, slot, func, 0);

        if(data != 0xffffffff) {
          uint32_t tmp = pci_read_word(bus, slot, func, 8);

          pci_devices[count].bus = bus;
          pci_devices[count].slot = slot;
          pci_devices[count].func = func;
          pci_devices[count].vendor_id = vendor_id(data);
          pci_devices[count].device_id = device_id(data);
          pci_devices[count].class = (tmp >> 24) & 0xff;
          pci_devices[count].subclass = (tmp >> 16) & 0xff;
          pci_devices[count].progif = (tmp >> 8) & 0xff;
          pci_devices[count].revision_id = tmp & 0xff;
          pci_devices[count].bar0 = pci_read_word(bus, slot, func, 0x10);
          pci_devices[count].bar1 = pci_read_word(bus, slot, func, 0x14);
          pci_devices[count].bar2 = pci_read_word(bus, slot, func, 0x18);
          pci_devices[count].bar3 = pci_read_word(bus, slot, func, 0x1c);
          pci_devices[count].bar4 = pci_read_word(bus, slot, func, 0x20);
          pci_devices[count].bar5 = pci_read_word(bus, slot, func, 0x24);
          count++;
        }
      }
    }
  }
  printf("%d devices detected\n", count);
  is_pci_ready = true;
}
