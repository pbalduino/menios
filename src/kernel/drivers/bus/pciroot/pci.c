#include <kernel/kernel.h>
#include <kernel/pci.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <types.h>

#define PCI_MMCONFIG_MAX_WINDOWS 16

typedef struct {
  uint64_t phys_base;
  volatile uint8_t* virt_base;
  uint16_t segment;
  uint8_t bus_start;
  uint8_t bus_end;
} pci_mmconfig_window_t;

static pci_mmconfig_window_t pci_mmconfig_windows[PCI_MMCONFIG_MAX_WINDOWS];
static size_t pci_mmconfig_window_count = 0;

static void pci_scan_range(uint16_t segment,
                           uint8_t bus_start,
                           uint8_t bus_end,
                           pci_enumerate_callback_t callback,
                           void* context);

void pci_mmconfig_reset(void) {
  pci_mmconfig_window_count = 0;
}

void pci_mmconfig_add_window(uint64_t base_phys,
                             uint16_t segment,
                             uint8_t bus_start,
                             uint8_t bus_end) {
  if(pci_mmconfig_window_count >= PCI_MMCONFIG_MAX_WINDOWS) {
    serial_printf("pci: ignoring MMCONFIG segment %u bus %u-%u (capacity exceeded)\n",
                  segment,
                  bus_start,
                  bus_end);
    return;
  }

  pci_mmconfig_window_t* window = &pci_mmconfig_windows[pci_mmconfig_window_count++];
  window->phys_base = base_phys;
  window->segment = segment;
  window->bus_start = bus_start;
  window->bus_end = bus_end;
  window->virt_base = (volatile uint8_t*)(uintptr_t)physical_to_virtual((phys_addr_t)base_phys);

  serial_printf("pci: registered MMCONFIG segment %u bus %u-%u @ phys 0x%llx virt %p\n",
                segment,
                bus_start,
                bus_end,
                (unsigned long long)base_phys,
                window->virt_base);
}

static inline pci_mmconfig_window_t* pci_mmconfig_find(uint16_t segment, uint8_t bus) {
  for(size_t i = 0; i < pci_mmconfig_window_count; ++i) {
    pci_mmconfig_window_t* window = &pci_mmconfig_windows[i];

    if(bus < window->bus_start || bus > window->bus_end) {
      continue;
    }

    if(window->segment != segment) {
      continue;
    }

    return window;
  }

  return NULL;
}

uint32_t pci_config_read_segment(uint16_t segment,
                                 uint8_t bus,
                                 uint8_t device,
                                 uint8_t function,
                                 uint16_t offset) {
  pci_mmconfig_window_t* window = pci_mmconfig_find(segment, bus);
  if(window != NULL) {
    uintptr_t base = (uintptr_t)window->virt_base;
    uintptr_t offset_in_window =
      ((uintptr_t)(bus - window->bus_start) << 20) |
      ((uintptr_t)device << 15) |
      ((uintptr_t)function << 12) |
      (offset & 0xffc);

    volatile uint32_t* reg = (volatile uint32_t*)(base + offset_in_window);
    return *reg;
  }

  uint32_t address = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                     ((uint32_t)function << 8) | (offset & 0xfc);
  outl(0xcf8, address); // Write address to CONFIG_ADDRESS
  return inl(0xcfc);    // Read data from CONFIG_DATA
}

void pci_config_write_segment(uint16_t segment,
                              uint8_t bus,
                              uint8_t device,
                              uint8_t function,
                              uint16_t offset,
                              uint32_t value) {
  pci_mmconfig_window_t* window = pci_mmconfig_find(segment, bus);
  if(window != NULL) {
    uintptr_t base = (uintptr_t)window->virt_base;
    uintptr_t offset_in_window =
      ((uintptr_t)(bus - window->bus_start) << 20) |
      ((uintptr_t)device << 15) |
      ((uintptr_t)function << 12) |
      (offset & 0xffc);

    volatile uint32_t* reg = (volatile uint32_t*)(base + offset_in_window);
    *reg = value;
    return;
  }

  uint32_t address = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                     ((uint32_t)function << 8) | (offset & 0xfc);
  outl(0xcf8, address); // Write address to CONFIG_ADDRESS
  outl(0xcfc, value);   // Write data to CONFIG_DATA
}

static void pci_scan_range(uint16_t segment,
                           uint8_t bus_start,
                           uint8_t bus_end,
                           pci_enumerate_callback_t callback,
                           void* context) {
  if(callback == NULL) {
    return;
  }

  for(uint16_t bus = bus_start; bus <= bus_end; ++bus) {
    for(uint16_t device = 0; device < 32; ++device) {
      uint32_t vendor_device = pci_config_read_segment(segment,
                                                       (uint8_t)bus,
                                                       (uint8_t)device,
                                                       0,
                                                       PCI_CONFIG_VENDOR_DEVICE);
      if((vendor_device & 0xFFFFu) == 0xFFFFu) {
        continue;
      }

      uint32_t header = pci_config_read_segment(segment,
                                                (uint8_t)bus,
                                                (uint8_t)device,
                                                0,
                                                PCI_CONFIG_HEADER_TYPE);
      uint8_t header_type = (uint8_t)((header >> 16) & 0xFFu);
      uint8_t function_limit = (header_type & PCI_HEADER_TYPE_MULTIFUNC) ? 8u : 1u;

      for(uint16_t function = 0; function < function_limit; ++function) {
        vendor_device = pci_config_read_segment(segment,
                                                (uint8_t)bus,
                                                (uint8_t)device,
                                                (uint8_t)function,
                                                PCI_CONFIG_VENDOR_DEVICE);
        if((vendor_device & 0xFFFFu) == 0xFFFFu) {
          continue;
        }

        uint32_t class_reg = pci_config_read_segment(segment,
                                                     (uint8_t)bus,
                                                     (uint8_t)device,
                                                     (uint8_t)function,
                                                     PCI_CONFIG_CLASSREV);

        pci_device_location_t location = {
          .segment = segment,
          .bus = (uint8_t)bus,
          .device = (uint8_t)device,
          .function = (uint8_t)function,
        };

        callback(&location, vendor_device, class_reg, context);
      }
    }
  }
}

void pci_enumerate_devices(pci_enumerate_callback_t callback, void* context) {
  if(callback == NULL) {
    return;
  }

  if(pci_mmconfig_window_count == 0) {
    pci_scan_range(0, 0, 255, callback, context);
    return;
  }

  for(size_t i = 0; i < pci_mmconfig_window_count; ++i) {
    pci_mmconfig_window_t* window = &pci_mmconfig_windows[i];
    pci_scan_range(window->segment,
                   window->bus_start,
                   window->bus_end,
                   callback,
                   context);
  }
}
