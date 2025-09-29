#ifndef MENIOS_INCLUDE_KERNEL_AHCI_H
#define MENIOS_INCLUDE_KERNEL_AHCI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/pmm.h>

typedef struct ahci_controller_t {
  uint8_t      bus;
  uint8_t      device;
  uint8_t      function;
  phys_addr_t  abar_phys;
  void*        abar;
  uint32_t     gsi;
  uint8_t      irq_line;
  uint8_t      irq_pin;
  bool         irq_configured;
  struct ahci_controller_t* next;
} ahci_controller_t;

typedef ahci_controller_t* ahci_controller_p;

void ahci_init(void);
ahci_controller_t* ahci_controllers(void);
size_t ahci_controller_count(void);
void ahci_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif
