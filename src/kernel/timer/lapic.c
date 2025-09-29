#include <kernel/apic.h>
#include <kernel/idt.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <stdio.h>

static uintptr_t lapic_addr;
static uint64_t lapic_counts_per_sec;
static uint32_t lapic_divider = DIV_BY_16;

void lapic_timer_init(void) {
  serial_printf("lapic timer init\n");
  lapic_addr = physical_to_virtual(DEFAULT_LAPIC_ADDRESS);
  serial_printf("lapic address: %lx - virt: %lx\n", DEFAULT_LAPIC_ADDRESS, lapic_addr);

  uint32_t svr = read_lapic(lapic_addr + LAPIC_SVR);
  write_lapic(lapic_addr + LAPIC_SVR, svr | 0x100); // enable LAPIC by setting spurious vector enable bit

  lapic_timer_set_divider(lapic_divider);
  lapic_timer_configure(ISR_PERIODIC_TIMER, false, true); // start masked in one-shot mode for calibration
  lapic_timer_set_initial_count(0);
  lapic_counts_per_sec = 0;
}

void lapic_timer_set_divider(uint32_t divider) {
  lapic_divider = divider;
  write_lapic(lapic_addr + LAPIC_TIMER_DIV, divider);
}

void lapic_timer_configure(uint8_t vector, bool periodic, bool masked) {
  uint32_t lvt = vector;
  if(periodic) {
    lvt |= (1u << 17);
  }
  if(masked) {
    lvt |= (1u << 16);
  }
  write_lapic(lapic_addr + LAPIC_TIMER_LVT, lvt);
}

void lapic_timer_set_initial_count(uint32_t count) {
  write_lapic(lapic_addr + LAPIC_TIMER_INIT, count);
}

uint32_t lapic_timer_current_count(void) {
  return read_lapic(lapic_addr + LAPIC_TIMER_CURR);
}

void lapic_timer_stop(void) {
  lapic_timer_set_initial_count(0);
}

void lapic_timer_set_counts_per_second(uint64_t counts) {
  lapic_counts_per_sec = counts;
}

uint64_t lapic_timer_counts_per_second(void) {
  return lapic_counts_per_sec;
}

void timer_eoi() {
  write_lapic(lapic_addr + LAPIC_EOI, 0);  // End Of Interrupt (EOI) to acknowledge
}
