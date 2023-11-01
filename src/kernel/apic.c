#include <kernel/apic.h>
#include <kernel/kernel.h>
#include <kernel/timer.h>
#include <stdint.h>
#include <stdio.h>

// Function to disable the PIC by masking all interrupts
void pic_disable() {
  printf("  Disabling legacy PIC");
  // Mask all interrupts on the master PIC
  printf(".");
  outb(PIC1_DATA_PORT, 0xff);

  // Mask all interrupts on the slave PIC
  printf(".");
  outb(PIC2_DATA_PORT, 0xff);

  printf(". OK\n");
}

void cpuid(uint32_t code, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
  asm volatile (
    "cpuid"
    : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
    : "a"(code)
  );
}

// Function to get the LAPIC base address for the nth processor
uint64_t get_lapic_base_address(uint32_t processor_number) {
  uint32_t max_cpuid;
  uint32_t eax, ebx, ecx, edx;

  // Get the maximum supported CPUID input value
  cpuid(0, &max_cpuid, &ebx, &ecx, &edx);

  if (max_cpuid < CPUID_INFO) {
    // CPUID not supported
    return 0;
  }

  // Check if the LAPIC is supported
  cpuid(CPUID_INFO, &eax, &ebx, &ecx, &edx);
  if (!(edx & (1 << 9))) {
    // LAPIC not supported
    return 0;
  }

  // Check if the processor number is valid
  if (processor_number >= (eax & 0xFF)) {
      // Invalid processor number
    return 0;
  }

  // Execute CPUID with EAX=4 to get processor information
  cpuid(4, &eax, &ebx, &ecx, &edx);

  // Calculate the LAPIC base address for the nth processor
  uint64_t lapic_base = ((uint64_t)ecx << 32) | edx;

  return lapic_base;
}

static inline void write_msr(uint32_t msr, uint64_t value) {
  asm volatile ("wrmsr" : : "c"(msr), "A"(value));
}

static inline uint64_t read_msr(uint32_t msr) {
  uint64_t value;
  asm volatile ("rdmsr" : "=A"(value) : "c"(msr));
  return value;
}

void lapic_init() {
  uint64_t apic_base = read_msr(LAPIC_BASE_MSR);
  apic_base |= (1 << 11);  // Set the LAPIC enable bit
  printf("  Enabling LAPIC.");
  write_msr(LAPIC_BASE_MSR, apic_base);
  printf(".. OK\n");
}

void apic_init() {
  printf("- Enabling APIC\n");
  pic_disable();
  lapic_init();
  timer_init();
}
