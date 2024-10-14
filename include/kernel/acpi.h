#ifndef MENIOS_INCLUDE_KERNEL_ACPI_H
#define MENIOS_INCLUDE_KERNEL_ACPI_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct acpi_rsdp_t {
 char     signature[8];
 uint8_t  checksum;
 char     oemid[6];
 uint8_t  revision;
 uint32_t rsdt_address;
} acpi_rsdp_t;

typedef struct acpi_xsdp_t {
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

typedef struct acpi_sdt_header_t {
  char     signature[4];
  uint32_t length;
  uint8_t  revision;
  uint8_t  checksum;
  char     oemid[6];
  char     oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct acpi_rsdt_t {
  acpi_sdt_header_t header;
  uint32_t* sdt;
} acpi_rsdt_t;

typedef struct acpi_xsdt_t {
  acpi_sdt_header_t header;
  uint64_t* sdt;
} acpi_xsdt_t;

typedef uint8_t acpi_address_space_t;

#define ACPI_SYSTEM_MEMORY 0
#define ACPI_SYSTEM_IO     1

#define ACPI_ACCESS_SIZE_UNDEFINED 0
#define ACPI_ACCESS_SIZE_BYTE      1
#define ACPI_ACCESS_SIZE_WORD      2
#define ACPI_ACCESS_SIZE_DWORD     3
#define ACPI_ACCESS_SIZE_QWORD     4

typedef struct acpi_address_t {
  uint8_t     space_id;
  uint8_t     register_bit_width;
  uint8_t     register_bit_offset;
  uint8_t     access_size;
  phys_addr_t address;
} acpi_address_t;

int acpi_init();
int acpi_shutdown();

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_KERNEL_ACPI_H */

