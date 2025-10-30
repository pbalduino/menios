#include <kernel/cpu.h>

static inline uint64_t read_cr0(void) {
  uint64_t value;
  asm volatile("mov %%cr0, %0" : "=r"(value));
  return value;
}

static inline uint64_t read_cr4(void) {
  uint64_t value;
  asm volatile("mov %%cr4, %0" : "=r"(value));
  return value;
}

static inline void write_cr0(uint64_t value) {
  asm volatile("mov %0, %%cr0" :: "r"(value) : "memory");
}

static inline void write_cr4(uint64_t value) {
  asm volatile("mov %0, %%cr4" :: "r"(value) : "memory");
}

void cpu_enable_sse(void) {
  uint64_t cr0 = read_cr0();
  cr0 &= ~(1ull << 2);  // Clear EM to enable FPU instructions.
  cr0 |= (1ull << 1);   // Set MP to enable WAIT/FWAIT instructions.
  cr0 &= ~(1ull << 3);  // Clear TS to avoid #NM on first use.
  write_cr0(cr0);

  uint64_t cr4 = read_cr4();
  cr4 |= (1ull << 9);   // OSFXSR - enable FXSAVE/FXRSTOR and SSE instructions.
  cr4 |= (1ull << 10);  // OSXMMEXCPT - enable SSE exception support.
  write_cr4(cr4);

  asm volatile("fninit");
}
