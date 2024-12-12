#include <kernel/console.h>
#include <kernel/serial.h>
#include <kernel/timer.h>
#include <kernel/tsc.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <types.h>

static inline void _cpuid(uint32_t eax, uint32_t ecx, uint32_t* regs) {
  __asm__ volatile("cpuid"
                  : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                  : "a"(eax), "c"(ecx));
}

static uint64_t boot_time_sec;
static uint64_t tick_start;

uint64_t frequency_ns = 0;

bool has_invariant_tsc() {
  uint32_t regs[4];
  
  // Call CPUID with function 0x80000007
  _cpuid(0x80000007, 0, regs);

  // Check if bit 8 of EDX is set (Invariant TSC)
  return (regs[3] & (1 << 8)) != 0;
}

uint64_t read_tsc(void) {
  uint32_t eax, edx;
  __asm__ volatile("rdtsc" : "=a"(eax), "=d"(edx));
  return ((uint64_t)edx << 32) | eax;
}

void tsc_init() {
  tick_start = read_tsc();
}

useconds_t unix_time_us() {
  uint64_t tsc = read_tsc() - tick_start;
  return ((boot_time_sec * 1000000000) + tsc) / 1000;
}

useconds_t ns_from_boot() {
  return read_tsc() - tick_start;
}