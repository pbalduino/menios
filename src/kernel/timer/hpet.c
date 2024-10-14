#include <kernel/hpet.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <uacpi/acpi.h>
#include <uacpi/namespace.h>
#include <uacpi/tables.h>
#include <uacpi/utilities.h>

hpet_status_t hpet_timer_init() {
  uacpi_table tbl;

  uacpi_status ret = uacpi_table_find_by_signature("APIC", &tbl);
  if (uacpi_unlikely_error(ret)) {
    serial_printf("unable to find HPET table: %s\n", uacpi_status_to_string(ret));
    return HPET_ERROR;
  }

  hpet_table_t *hpet = (hpet_table_t *)tbl.virt_addr;
  uint64_t hpet_base = hpet->address.address;

  serial_printf("Found HPET @ %lx\n", tbl.ptr);

  return HPET_ERROR;
}