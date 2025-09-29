#ifndef MENIOS_INCLUDE_KERNEL_PCI_H
#define MENIOS_INCLUDE_KERNEL_PCI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif
