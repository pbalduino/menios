#include <boot/limine.h>
#include <stdio.h>
#include <types.h>
#include <kernel/acpi.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>

static volatile struct limine_rsdp_request rsdp_request = {
  .id = LIMINE_RSDP_REQUEST,
  .revision = 0
};

static struct limine_rsdp_response *rsdp_response;
static acpi_xsdp_t* xsdp;

void acpi_init() {
  printf("- Initing device table.");
  if(rsdp_request.response == NULL) {
    printf(">>> Error loading device table.\n");
    hcf();
  }

  rsdp_response = rsdp_request.response;
  printf("..OK.\n");
  printf("  Found at: %p\n", &rsdp_response->address);
  xsdp = (acpi_xsdp_t*)rsdp_response->address;

  printf("  Signature: %s\n", xsdp->signature);
  // xsdp->xsdt_address
}
