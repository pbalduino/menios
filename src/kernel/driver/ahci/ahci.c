#include <kernel/ahci.h>
#include <kernel/dma.h>
#include <kernel/heap.h>
#include <kernel/pci.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <string.h>

#ifndef AHCI_VERBOSE_LOG
#define AHCI_VERBOSE_LOG 1
#endif

#define PCI_HEADER_TYPE_MULTIFUNC 0x80

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA 0x06
#define PCI_PROGIF_AHCI 0x01

#define PCI_CONFIG_VENDOR_DEVICE 0x00
#define PCI_CONFIG_STATUS_COMMAND 0x04
#define PCI_CONFIG_CLASSREV 0x08
#define PCI_CONFIG_BAR5 0x24
#define PCI_CONFIG_HEADER_TYPE 0x0C

#define PCI_COMMAND_MEMORY_SPACE (1u << 1)
#define PCI_COMMAND_BUS_MASTER   (1u << 2)

static ahci_controller_t* controllers_head = NULL;
static size_t controllers_count = 0;
static bool ahci_initialized = false;

static void ahci_register_controller(uint8_t bus,
                                     uint8_t device,
                                     uint8_t function,
                                     phys_addr_t abar_phys) {
  ahci_controller_t* node = kmalloc(sizeof(ahci_controller_t));
  if(node == NULL) {
    serial_printf("ahci: failed to allocate controller descriptor for %02x:%02x.%u\n",
                  bus,
                  device,
                  function);
    return;
  }

  memset(node, 0, sizeof(*node));
  node->bus = bus;
  node->device = device;
  node->function = function;
  node->abar_phys = abar_phys;
  node->abar = (void*)physical_to_virtual(abar_phys);
  node->next = controllers_head;
  controllers_head = node;
  controllers_count++;

#if AHCI_VERBOSE_LOG
  serial_printf("ahci: controller %02x:%02x.%u mapped at phys=%llx virt=%p\n",
                bus,
                device,
                function,
                (unsigned long long)abar_phys,
                node->abar);
#endif
}

static bool ahci_is_candidate(uint8_t bus, uint8_t device, uint8_t function) {
  uint32_t class_reg = pci_config_read(bus, device, function, PCI_CONFIG_CLASSREV);
  uint8_t class_code = (class_reg >> 24) & 0xFF;
  uint8_t subclass = (class_reg >> 16) & 0xFF;
  uint8_t prog_if = (class_reg >> 8) & 0xFF;

  return class_code == PCI_CLASS_MASS_STORAGE &&
         subclass == PCI_SUBCLASS_SATA &&
         (prog_if & 0x80 ? (prog_if & 0x7F) == PCI_PROGIF_AHCI : prog_if == PCI_PROGIF_AHCI);
}

static void ahci_enable_memory_and_busmaster(uint8_t bus, uint8_t device, uint8_t function) {
  uint32_t command = pci_config_read(bus, device, function, PCI_CONFIG_STATUS_COMMAND);
  command |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
  pci_config_write(bus, device, function, PCI_CONFIG_STATUS_COMMAND, command);
}

static void ahci_scan_bus(void) {
  for(uint16_t bus = 0; bus < 256; bus++) {
    for(uint16_t device = 0; device < 32; device++) {
      uint8_t pci_bus = (uint8_t)bus;
      uint8_t pci_device = (uint8_t)device;
      uint32_t vendor_device = pci_config_read(pci_bus, pci_device, 0, PCI_CONFIG_VENDOR_DEVICE);
      if((vendor_device & 0xFFFF) == 0xFFFF) {
        continue;
      }

      uint8_t header_type = (pci_config_read(pci_bus, pci_device, 0, PCI_CONFIG_HEADER_TYPE) >> 16) & 0xFF;
      uint8_t function_limit = (header_type & PCI_HEADER_TYPE_MULTIFUNC) ? 8 : 1;

      for(uint16_t function = 0; function < function_limit; function++) {
        uint8_t pci_function = (uint8_t)function;
        vendor_device = pci_config_read(pci_bus, pci_device, pci_function, PCI_CONFIG_VENDOR_DEVICE);
        if((vendor_device & 0xFFFF) == 0xFFFF) {
          continue;
        }

        if(!ahci_is_candidate(pci_bus, pci_device, pci_function)) {
          continue;
        }

        uint32_t bar5 = pci_config_read(pci_bus, pci_device, pci_function, PCI_CONFIG_BAR5);
        if((bar5 & 0xFFFFFFF0u) == 0) {
#if AHCI_VERBOSE_LOG
          serial_printf("ahci: controller %02x:%02x.%u missing BAR5\n", pci_bus, pci_device, pci_function);
#endif
          continue;
        }

        phys_addr_t abar_phys = (phys_addr_t)(bar5 & ~0xFu);
        ahci_enable_memory_and_busmaster(pci_bus, pci_device, pci_function);
        ahci_register_controller(pci_bus, pci_device, pci_function, abar_phys);
      }
    }
  }
}

void ahci_init(void) {
  if(ahci_initialized) {
    return;
  }

  controllers_head = NULL;
  controllers_count = 0;

  ahci_scan_bus();

  serial_printf("ahci: discovered %zu controller(s)\n", controllers_count);
  ahci_initialized = true;
}

ahci_controller_t* ahci_controllers(void) {
  return controllers_head;
}

size_t ahci_controller_count(void) {
  return controllers_count;
}
