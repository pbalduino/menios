#include <kernel/arch/x86_64/apic.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <stdio.h>

static uintptr_t addr;

static uint32_t timer_freq = 10000000;

void lapic_timer_initialize(void) {
  serial_printf("lapic timer init\n");
  addr = physical_to_virtual(DEFAULT_LAPIC_ADDRESS);
  serial_printf("lapic address: %lx - virt: %lx\n", DEFAULT_LAPIC_ADDRESS, addr);
  write_lapic(addr + LAPIC_SVR, read_lapic(addr + LAPIC_SVR) | 0x100);
  write_lapic(addr + LAPIC_TIMER_DIV, DIV_BY_8);
  timer_frequency(timer_freq);
  write_lapic(addr + LAPIC_TIMER_LVT, 0x20020);
}

void timer_frequency(uint32_t freq) {
  timer_freq = freq;
  serial_printf("timer frequency: %d\n", timer_freq);
  write_lapic(addr + LAPIC_TIMER_INIT, timer_freq);
}

void timer_eoi() {
  write_lapic(addr + LAPIC_EOI, 0);  // End Of Interrupt (EOI) to acknowledge
}
