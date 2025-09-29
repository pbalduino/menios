#include <boot/limine.h>

#include <kernel/apic.h>
#include <kernel/idt.h>
#include <kernel/console.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/rtc.h>
#include <kernel/hpet.h>
#include <kernel/serial.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <kernel/tsc.h>

#include <stdbool.h>
#include <stdio.h>
#include <types.h>

extern void reset_timer();

static volatile struct limine_boot_time_request boot_time_request = {
  .id = LIMINE_BOOT_TIME_REQUEST,
  .revision = 3
};

static uint64_t tick = 0;
static const uint32_t lapic_target_hz = 1000;
static uint32_t lapic_counts_per_tick = 0;

void (*callback[16])(void*);
int last_callback = 0;

void timer_handler(void* arg) {
  tick++;
  for(int i = 0; i < last_callback; i++) {
    if(callback[i] != NULL) {
      callback[i](arg);
    }
  }
  timer_eoi();
}

void register_timer_callback(void (*cb)(void*)) {
  callback[last_callback++] = cb;
}

static bool calibrate_lapic_with_hpet(uint64_t* counts_per_second_out) {
  if(!hpet_is_available()) {
    return false;
  }

  const uint64_t hpet_freq = hpet_frequency_hz();
  if(hpet_freq == 0) {
    return false;
  }

  lapic_timer_set_divider(DIV_BY_16);
  lapic_timer_configure(ISR_PERIODIC_TIMER, false, true);
  lapic_timer_set_initial_count(0);
  lapic_timer_set_initial_count(0xffffffffu);

  uint64_t target_ticks = hpet_freq / 100; // ~10ms
  if(target_ticks == 0) {
    target_ticks = hpet_freq / 1000; // ~1ms fallback
  }
  if(target_ticks == 0) {
    target_ticks = 1;
  }

  uint64_t start = hpet_read_counter();
  while((hpet_read_counter() - start) < target_ticks) {
    __asm__ volatile("pause");
  }

  uint32_t elapsed = 0xffffffffu - lapic_timer_current_count();
  lapic_timer_stop();

  if(elapsed == 0) {
    return false;
  }

  uint64_t counts_per_second = ((uint64_t)elapsed * hpet_freq) / target_ticks;
  if(counts_per_second == 0) {
    return false;
  }

  if(counts_per_second_out) {
    *counts_per_second_out = counts_per_second;
  }
  return true;
}

static bool calibrate_lapic_with_tsc(uint64_t* counts_per_second_out) {
  uint64_t tsc_freq = tsc_frequency_hz();
  if(tsc_freq == 0) {
    return false;
  }

  lapic_timer_set_divider(DIV_BY_16);
  lapic_timer_configure(ISR_PERIODIC_TIMER, false, true);
  lapic_timer_set_initial_count(0);
  lapic_timer_set_initial_count(0xffffffffu);

  uint64_t tsc_target = tsc_freq / 100; // ~10ms
  if(tsc_target == 0) {
    tsc_target = tsc_freq / 1000; // ~1ms fallback
  }
  if(tsc_target == 0) {
    tsc_target = 1;
  }

  uint64_t start = read_tsc();
  while((read_tsc() - start) < tsc_target) {
    __asm__ volatile("pause");
  }

  uint32_t elapsed = 0xffffffffu - lapic_timer_current_count();
  lapic_timer_stop();

  if(elapsed == 0) {
    return false;
  }

  uint64_t counts_per_second = ((uint64_t)elapsed * tsc_freq) / tsc_target;
  if(counts_per_second == 0) {
    return false;
  }

  if(counts_per_second_out) {
    *counts_per_second_out = counts_per_second;
  }
  return true;
}

void timer_init() {
  logk("Initing timer\n");
  for(int i = 0; i < 16; i++) {
    callback[i] = NULL;
  };

  logk("  Initing LAPIC timer\n");
  lapic_timer_init();

  bool hpet_ok = (hpet_timer_init() == HPET_OK);
  if(!hpet_ok) {
    errk("  HPET timer not supported.\n");
  }

  logk("  Initing TSC\n");
  tsc_init();

  if(!has_invariant_tsc()) {
    errk("  Invariant TSC not supported.\n");
  }

  uint64_t lapic_counts_per_sec = 0;
  bool calibrated = false;

  if(hpet_ok) {
    calibrated = calibrate_lapic_with_hpet(&lapic_counts_per_sec);
    if(calibrated) {
      logk("  LAPIC timer calibrated using HPET\n");
    }
  }

  if(!calibrated) {
    calibrated = calibrate_lapic_with_tsc(&lapic_counts_per_sec);
    if(calibrated) {
      logk("  LAPIC timer calibrated using TSC fallback\n");
    }
  }

  if(!calibrated) {
    lapic_counts_per_sec = (uint64_t)lapic_target_hz * 10000000ull;
    errk("  LAPIC calibration failed, using fallback configuration\n");
  }

  lapic_counts_per_tick = (uint32_t)(lapic_counts_per_sec / lapic_target_hz);
  if(lapic_counts_per_tick == 0) {
    lapic_counts_per_tick = 1;
  }

  lapic_timer_set_counts_per_second(lapic_counts_per_sec);
  lapic_timer_set_divider(DIV_BY_16);
  lapic_timer_configure(ISR_PERIODIC_TIMER, true, false);
  lapic_timer_set_initial_count(lapic_counts_per_tick);

  serial_printf("lapic_timer: %llu counts/sec, %u counts/tick at %u Hz\n",
                (unsigned long long)lapic_counts_per_sec,
                lapic_counts_per_tick,
                lapic_target_hz);
}

uint64_t boot_time() {
  if(boot_time_request.response == NULL || boot_time_request.response->boot_time == 0) {
    printf("Boot time not available, halting\n");
    halt();
    return 0;
  } else {
    return boot_time_request.response->boot_time;
  }
}
