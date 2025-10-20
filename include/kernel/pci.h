#ifndef MENIOS_INCLUDE_KERNEL_PCI_H
#define MENIOS_INCLUDE_KERNEL_PCI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint32_t pci_config_read_segment(uint16_t segment,
                                 uint8_t bus,
                                 uint8_t device,
                                 uint8_t function,
                                 uint8_t offset);
void pci_config_write_segment(uint16_t segment,
                              uint8_t bus,
                              uint8_t device,
                              uint8_t function,
                              uint8_t offset,
                              uint32_t value);

static inline uint32_t pci_config_read(uint8_t bus,
                                       uint8_t device,
                                       uint8_t function,
                                       uint8_t offset) {
  return pci_config_read_segment(0, bus, device, function, offset);
}

static inline void pci_config_write(uint8_t bus,
                                    uint8_t device,
                                    uint8_t function,
                                    uint8_t offset,
                                    uint32_t value) {
  pci_config_write_segment(0, bus, device, function, offset, value);
}

void pci_mmconfig_reset(void);
void pci_mmconfig_add_window(uint64_t base_phys,
                             uint16_t segment,
                             uint8_t bus_start,
                             uint8_t bus_end);

#ifdef __cplusplus
}
#endif

#endif
