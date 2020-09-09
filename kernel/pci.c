#include <types.h>
#include <stdio.h>
#include <x86.h>
#include <kernel/pci.h>

const uint32_t PCI_ENABLE_BIT     = 0x80000000;
const uint32_t PCI_CONFIG_ADDRESS = 0xcf8;
const uint32_t PCI_CONFIG_DATA    = 0xcfc;

/*
const char[] PCI_CLASS = {
    "Unclassified",
    "Mass Storage Controller",
    "Network Controller",
    "Display Controller",
    "Multimedia Controller",
    "Memory Controller",
    "Bridge Device",
    "Simple Communication Controller",
    "Base System Peripheral",
    "Input Device Controller",
    "Docking Station",
    "Processor",
    "Serial Bus Controller",
    "Wireless Controller",
    "Intelligent Controller",
    "Satellite Communication Controller",
    "Encryption Controller",
    "Signal Processing Controller",

}
*/

typedef struct {
  uint16_t vendor_id;
  uint16_t device_id;
} pci_device_t;

uint16_t vendor_id(uint32_t data) {
  return (uint16_t)(data & 0xffff);
}

uint16_t device_id(uint32_t data) {
  return (uint16_t)((data >> 16) & 0xffff);
}

uint32_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  // unsigned long flags;
  // local_irq_save(flags)
  uint32_t lbus  = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;
  uint32_t address = PCI_ENABLE_BIT | (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | offset);

  outl(PCI_CONFIG_ADDRESS, address);

  // local_irq_restore(flags);
  return inl(PCI_CONFIG_DATA);
}

int init_pci(void) {
  printf("Listing PCI devices:\n");

  for(int bus = 0; bus < 0xff; bus++) {
    for(int device = 0; device < 0xff; device++) {
      for(int func = 0; func < 8; func++) {
        uint32_t data = pci_read_word(bus, device, func, 0);

        if(data != 0xffffffff) {
          printf("  Bus %d, device %d, func %d: vendor=0x%x dev_id=0x%x\n", bus, device, func, vendor_id(data), device_id(data));

          uint32_t tmp = pci_read_word(bus, device, func, 8);
          uint32_t addr = pci_read_word(bus, device, func, 0x10);
          printf("  Class 0x%x, Subclass 0x%x, ProgIF 0x%x, RevID 0x%x, Addr 0x%x\n",
            (tmp >> 24) & 0xff,
            (tmp >> 16) & 0xff,
            (tmp >> 8) & 0xff,
            tmp & 0xff,
            addr >> 4);


        }
      }
    }
  }
  printf("\n");
  return 0;
}
