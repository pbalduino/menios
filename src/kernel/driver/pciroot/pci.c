#include <kernel/kernel.h>
#include <types.h>

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
  uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xfc);
  outl(0xcf8, address); // Write address to CONFIG_ADDRESS
  return inl(0xcfc);    // Read data from CONFIG_DATA
}

void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
  uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xfc);
  outl(0xcf8, address); // Write address to CONFIG_ADDRESS
  outl(0xcfc, value);   // Write data to CONFIG_DATA
}