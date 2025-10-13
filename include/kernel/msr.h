#ifndef MENIOS_INCLUDE_KERNEL_MSR_H
#define MENIOS_INCLUDE_KERNEL_MSR_H

#include <types.h>

#ifdef MENIOS_HOST_TEST

static inline uint64_t msr_read(uint32_t msr) {
  (void)msr;
  return 0;
}

static inline void msr_write(uint32_t msr, uint64_t value) {
  (void)msr;
  (void)value;
}

#else

static inline uint64_t msr_read(uint32_t msr) {
  uint32_t low;
  uint32_t high;
  __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
  return ((uint64_t)high << 32) | low;
}

static inline void msr_write(uint32_t msr, uint64_t value) {
  uint32_t low = (uint32_t)(value & 0xffffffffu);
  uint32_t high = (uint32_t)(value >> 32);
  __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

#endif

#define IA32_EFER               0xC0000080u
#define IA32_STAR               0xC0000081u
#define IA32_LSTAR              0xC0000082u
#define IA32_FMASK              0xC0000084u
#define IA32_GS_BASE            0xC0000101u
#define IA32_KERNEL_GS_BASE     0xC0000102u

#define IA32_EFER_SCE           (1u << 0)

#endif
