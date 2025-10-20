#include <errno.h>
#include <kernel/ahci.h>
#include <kernel/console.h>
#include <kernel/driver.h>
#include <kernel/acpi.h>
#include <kernel/pci.h>
#include <kernel/serial.h>

#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

static void pciroot_visit_device(const pci_device_location_t* location,
                                 uint32_t vendor_device,
                                 uint32_t class_reg,
                                 void* context) {
  (void)context;

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

    if(mcfg->header.length > header_size) {
      entry_count = (mcfg->header.length - header_size) / sizeof(struct acpi_mcfg_allocation);
    }

    serial_printf("pciroot: found MCFG with %zu window(s)\n", entry_count);

    for(size_t i = 0; i < entry_count; ++i) {
      const struct acpi_mcfg_allocation* entry = &mcfg->entries[i];

      serial_printf("  MCFG[%zu]: segment=%u bus=%u-%u base=0x%llx\n",
                    i,
                    entry->pci_segment_group,
                    entry->start_bus_number,
                    entry->end_bus_number,
                    (unsigned long long)entry->base_address);

      if(entry->start_bus_number > entry->end_bus_number) {
        serial_printf("  MCFG[%zu]: invalid bus range, skipping\n", i);
        continue;
      }

      pci_mmconfig_add_window(entry->base_address,
                              entry->pci_segment_group,
                              entry->start_bus_number,
                              entry->end_bus_number);
    }
  }

  ahci_init();
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
