#include <kernel/serial.h>
#include <kernel/timer.h>
#include <kernel/tsc.h>
#include <stdio.h>
#include <types.h>

static inline void _cpuid(uint32_t eax, uint32_t ecx, uint32_t* regs) {
  __asm__ volatile("cpuid"
                  : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                  : "a"(eax), "c"(ecx));
}

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

void init_tsc() {
  printf("- Initing TSC");

  if(!has_invariant_tsc()) {
    serial_printf("CPU does not support invariant TSC\n");
  }

  serial_line("");

  uint64_t start_time = unix_time();
  serial_line("");
  uint64_t start_tsc = read_tsc();
  serial_line("");
  uint64_t end_time;
  uint64_t end_tsc;

  puts(".");
  while(start_time == unix_time()) {
    start_tsc = read_tsc();
  }
  puts(".");

  serial_printf("init_tsc: starttime %ld\n", start_time);
  serial_printf("init_tsc: starttsc  %ld\n", start_tsc);

  start_time = unix_time();

  serial_printf("init_tsc: starttime %ld\n", start_time);
  
  uint64_t last_sec = start_time;

  while((end_time = unix_time()) < (start_time + 5)) {
    if(last_sec != end_time) {
      puts(".");
      last_sec = end_time;
    }
  }
  end_tsc = read_tsc();

  uint64_t frequency = (end_tsc - start_tsc) / 5;

  serial_printf("init_tsc: endtime  %ld\n", end_time);
  serial_printf("init_tsc: endtsc   %ld\n", end_tsc);
  serial_printf("init_tsc: tscdiff  %ld\n", end_tsc - start_tsc);
  serial_printf("init_tsc: timediff %ld\n", end_time - start_time);
  serial_printf("init_tsc: per sec  %ld\n", frequency);
  printf(".OK\n  Frequency: %ldfs\n", frequency);

}

