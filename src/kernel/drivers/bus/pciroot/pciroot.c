#include <errno.h>
#include <stdbool.h>
#include <kernel/ahci.h>
#include <kernel/console.h>
#include <kernel/driver.h>
#include <kernel/acpi.h>
#include <kernel/pci.h>
#include <kernel/serial.h>

#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

static bool pciroot_device_has_driver(uint32_t vendor_device, uint32_t class_reg) {
  (void)vendor_device;

  uint8_t base_class = (uint8_t)((class_reg >> 24) & 0xffu);
  uint8_t subclass = (uint8_t)((class_reg >> 16) & 0xffu);

  if(base_class == 0x01 && subclass == 0x06) {
    return true;  // AHCI SATA controller
  }

  return false;
}

static const char* pciroot_class_name(uint8_t base_class, uint8_t subclass, uint8_t prog_if) {
  switch(base_class) {
    case 0x00:
      return "Legacy device";
    case 0x01:
      switch(subclass) {
        case 0x00:
          return "SCSI controller";
        case 0x01:
          return "IDE controller";
        case 0x02:
          return "Floppy controller";
        case 0x03:
          return "IPI controller";
        case 0x04:
          return "RAID controller";
        case 0x05:
          return "ATA controller";
        case 0x06:
          return (prog_if == 0x01) ? "AHCI controller" : "SATA controller";
        case 0x07:
          return "Serial Attached SCSI controller";
        case 0x08:
          return "NVMe controller";
        default:
          return "Mass storage controller";
      }
    case 0x02:
      switch(subclass) {
        case 0x00:
          return "Ethernet controller";
        case 0x01:
          return "Token Ring controller";
        case 0x02:
          return "FDDI controller";
        case 0x03:
          return "ATM controller";
        default:
          return "Network controller";
      }
    case 0x03:
      switch(subclass) {
        case 0x00:
          return "VGA controller";
        case 0x01:
          return "XGA controller";
        case 0x02:
          return "3D controller";
        default:
          return "Display controller";
      }
    case 0x04:
      switch(subclass) {
        case 0x00:
          return "Video device";
        case 0x01:
          return "Audio device";
        case 0x02:
          return "Computer telephony device";
        case 0x03:
          return "HD Audio device";
        default:
          return "Multimedia device";
      }
    case 0x05:
      switch(subclass) {
        case 0x00:
          return "RAM controller";
        case 0x01:
          return "Flash controller";
        default:
          return "Memory controller";
      }
    case 0x06:
      switch(subclass) {
        case 0x00:
          return "Host bridge";
        case 0x01:
          return "ISA bridge";
        case 0x02:
          return "EISA bridge";
        case 0x04:
          return "PCI-to-PCI bridge";
        case 0x07:
          return "CardBus bridge";
        case 0x09:
          return "PCI-to-PCI bridge (secondary)";
        default:
          return "Bridge device";
      }
    case 0x07:
      switch(subclass) {
        case 0x00:
          return "Serial controller";
        case 0x01:
          return "Parallel controller";
        case 0x02:
          return "Multiport serial controller";
        default:
          return "Communication controller";
      }
    case 0x08:
      switch(subclass) {
        case 0x00:
          return "PIC";
        case 0x01:
          return "DMA controller";
        case 0x02:
          return "Timer";
        case 0x03:
          return "RTC";
        default:
          return "System peripheral";
      }
    case 0x09:
      return "Input controller";
    case 0x0A:
      return "Docking station";
    case 0x0B:
      return "Processor device";
    case 0x0C:
      switch(subclass) {
        case 0x00:
          return "FireWire controller";
        case 0x01:
          return "ACCESS.bus controller";
        case 0x02:
          return "SSA controller";
        case 0x03:
          switch(prog_if) {
            case 0x00:
              return "UHCI USB controller";
            case 0x10:
              return "OHCI USB controller";
            case 0x20:
              return "EHCI USB controller";
            case 0x30:
              return "xHCI USB controller";
            case 0x80:
              return "Generic USB controller";
            default:
              return "USB controller";
          }
        case 0x05:
          return "SMBus controller";
        default:
          return "Serial bus controller";
      }
    case 0x0D:
      return "Wireless controller";
    case 0x0E:
      return "Intelligent I/O controller";
    case 0x0F:
      return "Satellite communication controller";
    case 0x10:
      return "Encryption controller";
    case 0x11:
      return "Signal processing controller";
    default:
      return "Unknown device";
  }
}

static void pciroot_visit_device(const pci_device_location_t* location,
                                 uint32_t vendor_device,
                                 uint32_t class_reg,
                                 void* context) {
  (void)context;

  uint16_t vendor = (uint16_t)(vendor_device & 0xffffu);
  uint16_t device = (uint16_t)((vendor_device >> 16) & 0xffffu);
  uint8_t base_class = (uint8_t)((class_reg >> 24) & 0xffu);
  uint8_t subclass = (uint8_t)((class_reg >> 16) & 0xffu);
  uint8_t prog_if = (uint8_t)((class_reg >> 8) & 0xffu);

  const char* class_name = pciroot_class_name(base_class, subclass, prog_if);
  bool has_driver = pciroot_device_has_driver(vendor_device, class_reg);

  if(has_driver) {
    logk("PCI %02x:%02x.%x (%04x:%04x) class %02x/%02x/%02x - %s\n",
         location->bus,
         location->device,
         location->function,
         vendor,
         device,
         base_class,
         subclass,
         prog_if,
         class_name);
  } else {
    errk("PCI %02x:%02x.%x (%04x:%04x) class %02x/%02x/%02x - %s\n",
         location->bus,
         location->device,
         location->function,
         vendor,
         device,
         base_class,
         subclass,
         prog_if,
         class_name);
  }

  ahci_pci_probe(location, vendor_device, class_reg);
}

void pciroot_start(void) {
  pci_mmconfig_reset();

  uacpi_table tbl;
  uacpi_status ret = uacpi_table_find_by_signature("MCFG", &tbl);
  if(uacpi_unlikely_error(ret)) {
    serial_printf("pciroot: no MCFG table found (%s)\n", uacpi_status_to_string(ret));
  } else {
    const struct acpi_mcfg* mcfg = (const struct acpi_mcfg*)tbl.ptr;
    const size_t header_size = sizeof(*mcfg);
    size_t entry_count = 0;

    if(mcfg->hdr.length > header_size) {
      entry_count = (mcfg->hdr.length - header_size) / sizeof(struct acpi_mcfg_allocation);
    }

    serial_printf("pciroot: found MCFG with %zu window(s)\n", entry_count);

    for(size_t i = 0; i < entry_count; ++i) {
      const struct acpi_mcfg_allocation* entry = &mcfg->entries[i];

      serial_printf("  MCFG[%zu]: segment=%u bus=%u-%u base=0x%llx\n",
                    i,
                    entry->segment,
                    entry->start_bus,
                    entry->end_bus,
                    (unsigned long long)entry->address);

      if(entry->start_bus > entry->end_bus) {
        serial_printf("  MCFG[%zu]: invalid bus range, skipping\n", i);
        continue;
      }

      pci_mmconfig_add_window(entry->address,
                              entry->segment,
                              entry->start_bus,
                              entry->end_bus);
    }
  }

  ahci_init();
  logk("Enumerating PCI devices:\n");
  pci_enumerate_devices(pciroot_visit_device, NULL);
}

uint8_t pciroot_read(void) {
  // No byte-oriented interface yet; report nothing available.
  return 0;
}

void pciroot_write(void) {
}

int pciroot_ioctl(void* device, unsigned long request, void* argp) {
  (void)device;
  (void)request;
  (void)argp;
  return -ENOTTY;
}

void pciroot_shutdown(void) {
}

static struct driver_t pciroot_driver = {
  .hid = "PNP0A08",
  .name = "PCIe root bridge",
  .start = &pciroot_start,
  .read = &pciroot_read,
  .write = &pciroot_write,
  .ioctl = &pciroot_ioctl,
  .shutdown = &pciroot_shutdown
};

void pciroot_init(void) {
  driver_register(&pciroot_driver);
}
