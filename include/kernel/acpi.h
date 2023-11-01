#ifndef MENIOS_INCLUDE_KERNEL_ACPI_H
#define MENIOS_INCLUDE_KERNEL_ACPI_H

// #include <types.h>
// #include <boot/limine.h>
typedef struct {
  char     signature[8];
  uint8_t  checksum;
  char     oemid[6];
  uint8_t  revision;
  uint32_t rsdt_addr;      // deprecated since version 2.0
  uint32_t length;
  uint64_t xsdt_address;
  uint8_t  extended_checksum;
  uint8_t  reserved[3];
} acpi_xsdp_t;

typedef struct {
  char     signature[4];
  uint32_t length;
  uint8_t  revision;
  uint8_t  checksum;
  char     oemid[6];
  char     oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} acpi_sdt_header_t;

typedef struct {
  acpi_sdt_header_t header;
  uint64_t sdt_addr[];
} acpi_xsdt_t;

void acpi_init();



#endif
