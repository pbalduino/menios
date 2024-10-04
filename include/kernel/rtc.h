#ifndef MENIOS_INCLUDE_KERNEL_RTC_H
#define MENIOS_INCLUDE_KERNEL_RTC_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_INDEX_PORT 0x70
#define RTC_DATA_PORT  0x71

typedef struct rtc_time_t {
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;
  uint8_t day;
  uint8_t month;
  uint8_t year;
  uint16_t full_year;
  uint8_t weekday;
  uint8_t century;
  uint8_t register_b;
} rtc_time_t;

void rtc_time(rtc_time_t* time);

#ifdef __cplusplus
}
#endif


#endif // MENIOS_INCLUDE_KERNEL_RTC_H