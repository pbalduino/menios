#ifndef MENIOS_INCLUDE_KERNEL_TSC_H
#define MENIOS_INCLUDE_KERNEL_TSC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <types.h>

#define TICKS_PER_SECOND 1000000000

void tsc_init();

void tsc_override_calibration(uint64_t frequency_hz, uint64_t boot_seconds);

uint64_t read_tsc(void);

uint64_t tsc_ticks_to_ns(uint64_t ticks);
uint64_t tsc_ns_to_ticks(uint64_t ns);
uint64_t tsc_frequency_hz(void);

useconds_t unix_time_us();

useconds_t ns_from_boot();

bool has_invariant_tsc();

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_KERNEL_TSC_H
