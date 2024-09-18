#include <boot/limine.h>
#include <stdio.h>
#include <types.h>
#include <kernel/acpi.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

void acpi_init() {
  printf("- Initing device table.\n");
  serial_printf("acpi_init: Initing device table.\n");

  printf("..OK.\n");
}
