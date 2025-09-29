#include <kernel/ahci.h>
#include <kernel/console.h>
#include <kernel/driver.h>
#include <kernel/acpi.h>
#include <kernel/serial.h>

#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

void pciroot_start(void) {

  serial_printf("pciroot_start: Finding ACPI table (MCFG");
  uacpi_table tbl;

  uacpi_status ret = uacpi_table_find_by_signature("MCFG", &tbl);
  if(uacpi_unlikely_error(ret)) {
    serial_printf("unable to find ACPI table: %s\n", uacpi_status_to_string(ret));
    return;
  }

  serial_printf("found ACPI table: %s\n", uacpi_status_to_string(ret));
  logk(uacpi_status_to_string(ret));

  acpi_mcfg_t* mcfg = (acpi_mcfg_t*)tbl.ptr;

  serial_printf("pciroot_start: sign: %.4s - len: %d\n", mcfg->header.signature, mcfg->header.length);

  ahci_init();
}

uint8_t pciroot_read(void) {
  // No byte-oriented interface yet; report nothing available.
  return 0;
}

void pciroot_write(void) {
}

void pciroot_ioctl(void) {
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
  serial_printf("ps2kb_init: Registering driver '%s' for HID '%s'\n", pciroot_driver.name, pciroot_driver.hid);
  driver_register(&pciroot_driver);
}
