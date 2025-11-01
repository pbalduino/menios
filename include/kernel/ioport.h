#ifndef MENIOS_INCLUDE_KERNEL_IOPORT_H
#define MENIOS_INCLUDE_KERNEL_IOPORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ioport_region ioport_region_t;

void ioport_manager_initialize(void);
int ioport_reserve(uint16_t base, uint16_t length, ioport_region_t** out_region);
void ioport_release(ioport_region_t* region);
int ioport_read(ioport_region_t* region, uint16_t offset, uint8_t width, uint64_t* out_value);
int ioport_write(ioport_region_t* region, uint16_t offset, uint8_t width, uint64_t value);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_KERNEL_IOPORT_H */
