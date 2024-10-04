#include <kernel/kernel.h>
#include <kernel/rtc.h>
#include <kernel/serial.h>

#include <types.h>

uint8_t rtc_read(uint8_t reg) {
  outb(RTC_INDEX_PORT, reg);  // Select the register
  return inb(RTC_DATA_PORT);   // Read the data
}

uint8_t bcd_to_binary(uint8_t bcd) {
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

void rtc_time(rtc_time_t* time) {
  time->register_b = rtc_read(0x0b);
  
  bool bcd = !(time->register_b & 0x04);

  uint8_t seconds = rtc_read(0x00); // Read seconds
  uint8_t minutes = rtc_read(0x02); // Read minutes
  uint8_t weekday = rtc_read(0x06);
  uint8_t hours = rtc_read(0x04);   // Read hours
  uint8_t day = rtc_read(0x07);     // Read day of the month
  uint8_t month = rtc_read(0x08);   // Read month
  uint8_t year = rtc_read(0x09);    // Read year (offset from 1900)
  uint8_t century = rtc_read(0x32);

  time->seconds = bcd ? bcd_to_binary(seconds) : seconds;
  time->minutes = bcd ? bcd_to_binary(minutes) : minutes;
  time->hours = bcd ? bcd_to_binary(hours) : hours;

  if (!(time->register_b & 0x02) && (time->hours & 0x80)) {
    time->hours = ((time->hours & 0x7F) + 12) % 24;
  }

  time->day = bcd ? bcd_to_binary(day) : day;
  time->month = bcd ? bcd_to_binary(month) : month;
  time->century = bcd ? bcd_to_binary(century) : century;
  time->weekday = bcd ? bcd_to_binary(weekday) : weekday;
  time->year = (bcd ? bcd_to_binary(year) : year);
  time->full_year = time->year + (time->century * 100);
}