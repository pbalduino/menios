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
static uint64_t tsc_freq_hz;

static uint32_t cpuid_max_basic_leaf(void) {
  uint32_t regs[4];
  _cpuid(0, 0, regs);
  return regs[0];
}

bool has_invariant_tsc() {
  uint32_t regs[4];
  _cpuid(0x80000007, 0, regs);
  return (regs[3] & (1 << 8)) != 0;
}

uint64_t read_tsc(void) {
  uint32_t eax, edx;
  __asm__ volatile("rdtsc" : "=a"(eax), "=d"(edx));
  return ((uint64_t)edx << 32) | eax;
}

static void tsc_calibrate(void) {
  uint32_t regs[4];
  uint32_t max_basic = cpuid_max_basic_leaf();
  uint64_t freq = 0;

  if(max_basic >= 0x15) {
    _cpuid(0x15, 0, regs);
    uint32_t denom = regs[0];
    uint32_t numer = regs[1];
    uint32_t ref = regs[2];

    if(denom != 0 && numer != 0) {
      if(ref != 0) {
        freq = ((uint64_t)ref * numer) / denom;
      }
    }
  }

  if(freq == 0 && max_basic >= 0x16) {
    _cpuid(0x16, 0, regs);
    uint32_t base_mhz = regs[0];
    if(base_mhz != 0) {
      freq = (uint64_t)base_mhz * 1000000ull;
    }
  }

  if(freq == 0) {
    freq = 1000000000ull;
    serial_printf("tsc_calibrate: fallback frequency 1GHz\n");
  }

  tsc_freq_hz = freq;
  serial_printf("tsc_calibrate: TSC frequency %llu Hz\n", (unsigned long long)tsc_freq_hz);
}

static void mul_u64(uint64_t a, uint64_t b, uint64_t* hi, uint64_t* lo) {
  __uint128_t product = (__uint128_t)a * (__uint128_t)b;
  *hi = (uint64_t)(product >> 64);
  *lo = (uint64_t)product;
}

static uint64_t div_u128_u64(uint64_t hi, uint64_t lo, uint64_t div) {
  uint64_t quotient = 0;
  uint64_t remainder = hi;

  for(int i = 0; i < 64; i++) {
    remainder = (remainder << 1) | (lo >> 63);
    lo <<= 1;
    quotient <<= 1;
    if(remainder >= div) {
      remainder -= div;
      quotient |= 1;
    }
  }

  return quotient;
}

static inline void add_u64_to_u128(uint64_t* hi, uint64_t* lo, uint64_t add) {
  uint64_t new_lo = *lo + add;
  if(new_lo < *lo) {
    (*hi)++;
  }
  *lo = new_lo;
}

static inline uint64_t tsc_ticks_to_ns_internal(uint64_t ticks) {
  if(tsc_freq_hz == 0) {
    return ticks;
  }
  uint64_t hi, lo;
  mul_u64(ticks, 1000000000ull, &hi, &lo);
  return div_u128_u64(hi, lo, tsc_freq_hz);
}

static inline uint64_t tsc_ns_to_ticks_internal(uint64_t ns) {
  if(tsc_freq_hz == 0) {
    return ns;
  }
  uint64_t hi, lo;
  mul_u64(ns, tsc_freq_hz, &hi, &lo);
  return div_u128_u64(hi, lo, 1000000000ull);
}

void tsc_init() {
  boot_time_sec = boot_time();
  tsc_calibrate();
  tick_start = read_tsc();
}

void tsc_override_calibration(uint64_t frequency_hz, uint64_t boot_seconds) {
  tsc_freq_hz = frequency_hz;
  boot_time_sec = boot_seconds;
  tick_start = read_tsc();
}

uint64_t tsc_ticks_to_ns(uint64_t ticks) {
  return tsc_ticks_to_ns_internal(ticks);
}

uint64_t tsc_ns_to_ticks(uint64_t ns) {
  return tsc_ns_to_ticks_internal(ns);
}

uint64_t tsc_frequency_hz(void) {
  return tsc_freq_hz;
}

useconds_t unix_time_us() {
  uint64_t tsc_delta = read_tsc() - tick_start;
  uint64_t ns = tsc_ticks_to_ns_internal(tsc_delta);
  uint64_t hi, lo;
  mul_u64(boot_time_sec, 1000000000ull, &hi, &lo);
  add_u64_to_u128(&hi, &lo, ns);
  return (useconds_t)div_u128_u64(hi, lo, 1000ull);
}

useconds_t ns_from_boot() {
  uint64_t tsc_delta = read_tsc() - tick_start;
  return tsc_ticks_to_ns_internal(tsc_delta);
}
