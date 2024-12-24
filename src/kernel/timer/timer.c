#include <boot/limine.h>

#include <kernel/apic.h>
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

#include <stdio.h>
#include <types.h>

extern void reset_timer();

static volatile struct limine_boot_time_request boot_time_request = {
  .id = LIMINE_BOOT_TIME_REQUEST,
  .revision = 3
};

static uint64_t tick = 0;

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

void timer_init() {
  logk("Initing timer\n");
  for(int i = 0; i < 16; i++) {
    callback[i] = NULL;
  };

  logk("  Initing LAPIC timer\n");
  lapic_timer_init();

  logk("  Initing TSC\n");
  tsc_init();

  if(!has_invariant_tsc()) {
    errk("  Invariant TSC not supported.\n");
  } else {
    logk("  Invariant TSC supported, but ignored.\n");
  }

  if(hpet_timer_init() == HPET_OK) {
    errk("  HPET timer supported, but ignored.\n");
  } else {
    errk("  HPET timer not supported.\n");
  }
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