#ifndef MENIOS_INCLUDE_KERNEL_AHCI_H
#define MENIOS_INCLUDE_KERNEL_AHCI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <kernel/pmm.h>

typedef struct ahci_controller_t {
  uint8_t      bus;
  uint8_t      device;
  uint8_t      function;
  phys_addr_t  abar_phys;
  void*        abar;
  struct ahci_controller_t* next;
} ahci_controller_t;

typedef ahci_controller_t* ahci_controller_p;

void ahci_init(void);
ahci_controller_t* ahci_controllers(void);
size_t ahci_controller_count(void);

#ifdef __cplusplus
}
#endif

#endif
