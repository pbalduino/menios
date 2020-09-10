#ifndef MENIOS_INCLUDE_KERNEL_PCI_H
#define MENIOS_INCLUDE_KERNEL_PCI_H

#include <types.h>

#define PCI_MAX_DEVICES    0x10

typedef struct {
  uint8_t bus;
  uint8_t slot;
  uint8_t func;
  uint8_t class;
  uint8_t subclass;
  uint8_t progif;
  uint8_t revision_id;
  uint16_t vendor_id;
  uint16_t device_id;
  uint16_t status;
  uint16_t command;
  uint16_t bar0;
  uint16_t bar2;
} pci_device_t;

pci_device_t pci_devices[PCI_MAX_DEVICES];

bool is_pci_ready;

void init_pci();

#endif
