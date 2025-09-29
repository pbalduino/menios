#include <kernel/ahci.h>
#include <kernel/apic.h>
#include <kernel/dma.h>
#include <kernel/heap.h>
#include <kernel/idt.h>
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
#define PCI_CONFIG_INTERRUPT_LINE 0x3C

#define PCI_COMMAND_MEMORY_SPACE (1u << 1)
#define PCI_COMMAND_BUS_MASTER   (1u << 2)

#define AHCI_INVALID_GSI 0xFFFFFFFFu
#define AHCI_GHC_IE      (1u << 1)
#define AHCI_GHC_AE      (1u << 31)

typedef struct ahci_hba_port_t {
  uint32_t clb;
  uint32_t clbu;
  uint32_t fb;
  uint32_t fbu;
  uint32_t is;
  uint32_t ie;
  uint32_t cmd;
  uint32_t reserved0;
  uint32_t tfd;
  uint32_t sig;
  uint32_t ssts;
  uint32_t sctl;
  uint32_t serr;
  uint32_t sact;
  uint32_t ci;
  uint32_t sntf;
  uint32_t fbs;
  uint32_t reserved1[11];
  uint32_t vendor[4];
} ahci_hba_port_t;

typedef struct ahci_hba_mem_t {
  uint32_t cap;
  uint32_t ghc;
  uint32_t is;
  uint32_t pi;
  uint32_t vs;
  uint32_t ccc_ctl;
  uint32_t ccc_pts;
  uint32_t em_loc;
  uint32_t em_ctl;
  uint32_t cap2;
  uint32_t bohc;
  uint8_t  reserved[0x74];
  uint8_t  vendor[0x60];
  ahci_hba_port_t ports[32];
} ahci_hba_mem_t;

static ahci_controller_t* controllers_head = NULL;
static size_t controllers_count = 0;
static bool ahci_initialized = false;

static bool ahci_is_gsi_configured(uint32_t gsi);
static void ahci_controller_enable_interrupts(ahci_controller_t* controller);
static void ahci_configure_controller_interrupts(ahci_controller_t* controller);

static void ahci_register_controller(uint8_t bus,
                                     uint8_t device,
                                     uint8_t function,
                                     phys_addr_t abar_phys,
                                     uint8_t irq_line,
                                     uint8_t irq_pin) {
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
  node->irq_line = irq_line;
  node->irq_pin = irq_pin;
  node->gsi = (irq_line == 0xFF) ? AHCI_INVALID_GSI : (uint32_t)irq_line;
  node->irq_configured = false;
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

  ahci_configure_controller_interrupts(node);
}

static bool ahci_is_gsi_configured(uint32_t gsi) {
  if(gsi == AHCI_INVALID_GSI) {
    return false;
  }

  for(ahci_controller_t* node = controllers_head; node != NULL; node = node->next) {
    if(node->gsi == gsi && node->irq_configured) {
      return true;
    }
  }

  return false;
}

static void ahci_controller_enable_interrupts(ahci_controller_t* controller) {
  if(controller == NULL || controller->abar == NULL) {
    return;
  }

  volatile ahci_hba_mem_t* hba = (volatile ahci_hba_mem_t*)controller->abar;

  uint32_t ghc = hba->ghc;
  ghc |= AHCI_GHC_IE | AHCI_GHC_AE;
  hba->ghc = ghc;

  uint32_t pending = hba->is;
  if(pending != 0) {
    hba->is = pending;
  }

  uint32_t implemented = hba->pi;
  for(uint32_t port = 0; port < 32; port++) {
    if((implemented & (1u << port)) == 0) {
      continue;
    }

    volatile ahci_hba_port_t* port_regs = &hba->ports[port];
    uint32_t port_pending = port_regs->is;
    if(port_pending != 0) {
      port_regs->is = port_pending;
    }

    port_regs->ie = 0xFFFFFFFFu;
  }
}

static void ahci_configure_controller_interrupts(ahci_controller_t* controller) {
  if(controller == NULL) {
    return;
  }

  if(controller->gsi == AHCI_INVALID_GSI) {
    serial_printf("ahci: controller %02x:%02x.%u has no valid IRQ line (pin=%u)\n",
                  controller->bus,
                  controller->device,
                  controller->function,
                  controller->irq_pin);
    return;
  }

  bool already_configured = ahci_is_gsi_configured(controller->gsi);
  bool mapped = already_configured;

  if(!already_configured) {
    mapped = apic_configure_irq(controller->gsi, ISR_AHCI, true, true);
  }

  if(!mapped) {
    serial_printf("ahci: controller %02x:%02x.%u failed to route IRQ (GSI %u)\n",
                  controller->bus,
                  controller->device,
                  controller->function,
                  controller->gsi);
    return;
  }

  ahci_controller_enable_interrupts(controller);
  controller->irq_configured = true;

#if AHCI_VERBOSE_LOG
  serial_printf("ahci: controller %02x:%02x.%u interrupts enabled on vector 0x%02x (GSI %u)%s\n",
                controller->bus,
                controller->device,
                controller->function,
                ISR_AHCI,
                controller->gsi,
                already_configured ? " (shared)" : "");
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

        uint32_t intr_line = pci_config_read(pci_bus, pci_device, pci_function, PCI_CONFIG_INTERRUPT_LINE);
        uint8_t irq_line = (uint8_t)(intr_line & 0xFF);
        uint8_t irq_pin = (uint8_t)((intr_line >> 8) & 0xFF);

        phys_addr_t abar_phys = (phys_addr_t)(bar5 & ~0xFu);
        ahci_enable_memory_and_busmaster(pci_bus, pci_device, pci_function);
        ahci_register_controller(pci_bus, pci_device, pci_function, abar_phys, irq_line, irq_pin);
      }
    }
  }
}

void ahci_irq_handler(void) {
  bool serviced = false;

  for(ahci_controller_t* controller = controllers_head; controller != NULL; controller = controller->next) {
    if(!controller->irq_configured || controller->abar == NULL) {
      continue;
    }

    volatile ahci_hba_mem_t* hba = (volatile ahci_hba_mem_t*)controller->abar;
    uint32_t pending = hba->is;

    if(pending == 0) {
      continue;
    }

    serviced = true;

    hba->is = pending;

    uint32_t implemented = hba->pi;
    uint32_t active_ports = pending & implemented;

    for(uint32_t port = 0; port < 32; port++) {
      if(((active_ports >> port) & 0x1u) == 0) {
        continue;
      }

      volatile ahci_hba_port_t* port_regs = &hba->ports[port];
      uint32_t port_pending = port_regs->is;

      if(port_pending != 0) {
        port_regs->is = port_pending;
      }

#if AHCI_VERBOSE_LOG
      serial_printf("ahci: irq ctrl %02x:%02x.%u port %u pending=0x%08x\n",
                    controller->bus,
                    controller->device,
                    controller->function,
                    port,
                    port_pending);
#endif
    }
  }

  apic_send_eoi();

#if AHCI_VERBOSE_LOG
  if(!serviced) {
    serial_printf("ahci: spurious interrupt (no controller reported pending bits)\n");
  }
#endif
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
