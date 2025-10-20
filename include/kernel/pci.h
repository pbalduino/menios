#ifndef MENIOS_INCLUDE_KERNEL_PCI_H
#define MENIOS_INCLUDE_KERNEL_PCI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PCI_HEADER_TYPE_MULTIFUNC 0x80

#define PCI_COMMAND_MEMORY_SPACE (1u << 1)
#define PCI_COMMAND_BUS_MASTER   (1u << 2)

#define PCI_CONFIG_VENDOR_DEVICE   0x00
#define PCI_CONFIG_STATUS_COMMAND  0x04
#define PCI_CONFIG_CLASSREV        0x08
#define PCI_CONFIG_HEADER_TYPE     0x0C
#define PCI_CONFIG_BAR0            0x10
#define PCI_CONFIG_BAR1            0x14
#define PCI_CONFIG_BAR2            0x18
#define PCI_CONFIG_BAR3            0x1C
#define PCI_CONFIG_BAR4            0x20
#define PCI_CONFIG_BAR5            0x24
#define PCI_CONFIG_INTERRUPT_LINE  0x3C

typedef struct {
  uint16_t segment;
  uint8_t  bus;
  uint8_t  device;
  uint8_t  function;
} pci_device_location_t;

typedef void (*pci_enumerate_callback_t)(const pci_device_location_t* location,
                                         uint32_t vendor_device,
                                         uint32_t class_revision,
                                         void* context);

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

void pci_enumerate_devices(pci_enumerate_callback_t callback, void* context);

#ifdef __cplusplus
}
#endif

#endif
