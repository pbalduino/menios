#include <boot/limine.h>
#include <stdio.h>
#include <types.h>
#include <kernel/acpi.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#define _AcpiModuleName __FUNCTION__

#include <acpi.h>
#include <acpixf.h>
#include <acoutput.h>


int acpi_init() {
  ACPI_STATUS status;
  printf("- Initing ACPI.");
  serial_printf("acpi_init: Initializing ACPICA.\n");

  ACPI_FUNCTION_NAME("acpi_init");

  serial_printf("acpi_init: Initializing ACPICA subsystem.\n");
  status = AcpiInitializeSubsystem();
  if(ACPI_FAILURE(status)) {
    ACPI_EXCEPTION((AE_INFO, status, "While initializing ACPICA"));
    return status;
  }
  printf(".");

  // Initialize the ACPICA Table Manager and get all ACPI tables
  // serial_printf("acpi_init: Initializing Table Manager.\n");
  // status = AcpiInitializeTables(NULL, 16, FALSE);
  // if(ACPI_FAILURE(status)) {
  //   ACPI_EXCEPTION((AE_INFO, status, "While initializing Table Manager"));
  //   return status;
  // }
  // printf(".");

  // serial_printf("acpi_init: Loading ACPI tables.\n");
  // status = AcpiLoadTables();
  // if(ACPI_FAILURE(status)) {
  //   ACPI_EXCEPTION((AE_INFO, status, "While loading ACPI tables"));
  //   return status;
  // }
  // printf(".");
/*
  // Install local handlers
  // status = InstallHandlers();
  // if(ACPI_FAILURE(status)) {
  //   ACPI_EXCEPTION((AE_INFO, status, "While installing handlers"));
  //   return status;
  // }
  // printf(".");

  // Initialize the ACPI hardware
  status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
  if(ACPI_FAILURE(status)) {
    ACPI_EXCEPTION((AE_INFO, status, "While enabling ACPICA"));
    return(status);
  }

  // Complete the ACPI namespace object initialization
  status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
  if(ACPI_FAILURE(status)) {
    ACPI_EXCEPTION((AE_INFO, status, "While initializing ACPICA objects"));
    return status;
  }
*/
  return AE_OK;
}

void acpi_shutdown() {
  printf("- Shutting down.\n");
  serial_printf("acpi_shutdown: Shutting down.\n");

  // Enter sleep state
  AcpiEnterSleepStatePrep(5);
  cli(); // disable interrupts
  printf("  Shuting down.\n");
  AcpiEnterSleepState(5);
}