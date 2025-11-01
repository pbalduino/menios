#include <kernel/arch/x86_64/apic.h>
#include <kernel/serial.h>

#include <stdio.h>

static uint32_t timer_freq = 10000000;

void lapic_timer_initialize(void) {
  void* base = apic_get_lapic_base();
  serial_printf("lapic timer init (x2apic=%s) base=%p\n",
                apic_is_x2apic_enabled() ? "yes" : "no",
                base);
  write_lapic(LAPIC_SVR, read_lapic(LAPIC_SVR) | 0x100);
  write_lapic(LAPIC_TIMER_DIV, DIV_BY_8);
  timer_frequency(timer_freq);
  write_lapic(LAPIC_TIMER_LVT, 0x20020);
}

void timer_frequency(uint32_t freq) {
  timer_freq = freq;
  serial_printf("timer frequency: %d\n", timer_freq);
  write_lapic(LAPIC_TIMER_INIT, timer_freq);
}

void timer_eoi() {
  write_lapic(LAPIC_EOI, 0);  // End Of Interrupt (EOI) to acknowledge
}
