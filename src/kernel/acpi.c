#include <boot/limine.h>
#include <types.h>
#include <kernel/acpi.h>

static volatile struct limine_rsdp_request rsdp_request = {
  .id = LIMINE_RSDP_REQUEST,
  .revision = 0
};

void acpi_init() {

}
