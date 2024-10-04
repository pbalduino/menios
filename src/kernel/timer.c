#include <kernel/apic.h>
#include <kernel/console.h>
#include <kernel/pmm.h>
#include <kernel/rtc.h>
#include <kernel/serial.h>
#include <kernel/timer.h>

#include <stdio.h>
#include <types.h>

extern void reset_timer();

static uint64_t tick = 0;
static uint64_t timer_freq = 126582;
static uintptr_t addr;

void timer_frequency(uint32_t freq) {
  timer_freq = freq;
  serial_printf("timer frequency: %d\n", timer_freq);
  write_lapic(addr + LAPIC_TIMER_INIT, timer_freq);
}

void timer_enable() {
  write_lapic(addr + LAPIC_SVR, read_lapic(addr + LAPIC_SVR) | 0x100);
  write_lapic(addr + LAPIC_TIMER_DIV, DIV_BY_128);
  timer_frequency(timer_freq);
  write_lapic(addr + LAPIC_TIMER_LVT, 0x20020);
}

void timer_handler() {
  tick++;

  rtc_time_t time;
  rtc_time(&time);

  screen_pos_t pos;
  get_cursor_pos(&pos);
  gotoxy(118, 0);
  printf("%d-%d-%d %d:%d:%d UTC\n", time.full_year, time.month, time.day, time.hours, time.minutes, time.seconds);
  gotoxy(pos.x, pos.y);

  write_lapic(addr + LAPIC_EOI, 0);  // End Of Interrupt (EOI) to acknowledge
}

void timer_init() {
  printf("- Initing timer");
  addr = physical_to_virtual(0xFEE00000);
  timer_enable();
  puts(".");

  printf(".OK\n");
}

uint64_t unix_time() {
  rtc_time_t time;
  rtc_time(&time);

  uint32_t year = time.full_year; // e.g., 2023
  uint32_t month = time.month; // 1-12
  uint32_t day = time.day; // 1-31
  uint32_t hour = time.hours; // 0-23
  uint32_t minute = time.minutes; // 0-59
  uint32_t second = time.seconds; // 0-59
  uint32_t total_days = 0;

  static const uint32_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  for (uint32_t y = 1970; y < year; y++) {
    total_days += 365;
    // Check for leap year
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
      total_days++;
    }
  }

  for (uint32_t m = 1; m < month; m++) {
    total_days += days_in_month[m];
  }

  total_days += day - 1;

  if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
    total_days++;
  }

  uint64_t total_seconds = total_days * 86400;
  total_seconds += hour * 3600;
  total_seconds += minute * 60;
  total_seconds += second;

  return total_seconds;
}