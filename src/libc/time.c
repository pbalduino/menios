#ifndef MENIOS_KERNEL
#include <errno.h>
#endif
#include <menios/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#ifdef MENIOS_HOST_TEST
#include <dlfcn.h>
#include <errno.h>

static void* resolve_host_symbol(const char* name) {
  void* sym = dlsym(RTLD_NEXT, name);
  if(sym == NULL) {
    sym = dlsym(RTLD_DEFAULT, name);
  }
  return sym;
}
#endif

#define ASCTIME_BUFFER_SIZE 26

static const char* const WEEKDAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* const MONTH_NAMES[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char* const WEEKDAY_FULL_NAMES[] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};
static const char* const MONTH_FULL_NAMES[] = {
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December"
};
static const char* const AM_PM_STRINGS[] = {"AM", "PM"};

#define SECONDS_PER_MINUTE 60
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24
#define SECONDS_PER_HOUR (SECONDS_PER_MINUTE * MINUTES_PER_HOUR)
#define SECONDS_PER_DAY ((time_t)SECONDS_PER_HOUR * HOURS_PER_DAY)

static int64_t days_from_civil(int64_t year, unsigned month, unsigned day) {
  year -= (month <= 2);
  const int64_t era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = (unsigned)(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t days,
                            int64_t* out_year,
                            unsigned* out_month,
                            unsigned* out_day,
                            unsigned* out_yday) {
  days += 719468;
  int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  unsigned doe = (unsigned)(days - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t year = (int64_t)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  unsigned day = doy - (153 * mp + 2) / 5 + 1;
  unsigned month = mp + (mp < 10 ? 3 : -9);
  year += (month <= 2);
  static const unsigned month_offsets[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  unsigned yday = month_offsets[month - 1] + (day - 1);
  int is_leap = 0;
  if((year % 4) == 0) {
    if((year % 100) != 0 || (year % 400) == 0) {
      is_leap = 1;
    }
  }
  if(is_leap && month > 2) {
    yday += 1;
  }
  *out_year = year;
  *out_month = month;
  *out_day = day;
  *out_yday = yday;
}

static int64_t floor_div(int64_t a, int64_t b) {
  int64_t q = a / b;
  int64_t r = a % b;
  if((r != 0) && ((r > 0) != (b > 0))) {
    q--;
  }
  return q;
}

static int64_t floor_mod(int64_t a, int64_t b) {
  int64_t r = a % b;
  if((r != 0) && ((r > 0) != (b > 0))) {
    r += b;
  }
  return r;
}

#ifndef MENIOS_HOST_TEST
#include <menios/syscall_user.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
extern long syscall(long number, ...);
#endif

time_t time(time_t* tloc) {
#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall1(SYS_TIME, (long)tloc);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return (time_t)-1;
  }
  return (time_t)rc;
#else
#ifdef SYS_time
  long rc = syscall(SYS_time, tloc);
  if(rc == -1) {
    return (time_t)-1;
  }
  return (time_t)rc;
#else
  (void)tloc;
  return (time_t)-1;
#endif
#endif
}

int gettimeofday(struct timeval* tv, struct timezone* tz) {
#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall2(SYS_GETTIMEOFDAY, (long)tv, (long)tz);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return -1;
  }
  return 0;
#else
#ifdef SYS_gettimeofday
  long rc = syscall(SYS_gettimeofday, tv, tz);
  if(rc == -1) {
    return -1;
  }
  return 0;
#else
  (void)tv;
  (void)tz;
  return -1;
#endif
#endif
}

static time_t tm_to_seconds(const struct tm* tm) {
  int64_t year = tm->tm_year + 1900;
  int64_t month = tm->tm_mon;
  int64_t day = tm->tm_mday;

  int64_t month_carry = floor_div(month, 12);
  month -= month_carry * 12;
  if(month < 0) {
    month += 12;
    month_carry--;
  }
  year += month_carry;

  int64_t seconds = (int64_t)tm->tm_sec + (int64_t)tm->tm_min * SECONDS_PER_MINUTE + (int64_t)tm->tm_hour * SECONDS_PER_HOUR;
  int64_t day_carry = floor_div(seconds, SECONDS_PER_DAY);
  int64_t sec_of_day = floor_mod(seconds, SECONDS_PER_DAY);

  int64_t days = days_from_civil(year, (unsigned)month + 1, 1);
  days += (day - 1) + day_carry;

  int64_t total = days * (int64_t)SECONDS_PER_DAY + sec_of_day;
  time_t result = (time_t)total;
  if((int64_t)result != total) {
    return (time_t)-1;
  }
  return result;
}

static void seconds_to_tm(time_t value, struct tm* tm) {
  int64_t seconds = (int64_t)value;
  int64_t days = floor_div(seconds, SECONDS_PER_DAY);
  int64_t rem = floor_mod(seconds, SECONDS_PER_DAY);

  tm->tm_hour = (int)(rem / SECONDS_PER_HOUR);
  rem %= SECONDS_PER_HOUR;
  tm->tm_min = (int)(rem / SECONDS_PER_MINUTE);
  tm->tm_sec = (int)(rem % SECONDS_PER_MINUTE);

  int wday = (int)((days + 4) % 7);
  if(wday < 0) {
    wday += 7;
  }
  tm->tm_wday = wday;

  int64_t year;
  unsigned month;
  unsigned day;
  unsigned yday;
  civil_from_days(days, &year, &month, &day, &yday);
  tm->tm_year = (int)(year - 1900);
  tm->tm_mon = (int)(month - 1);
  tm->tm_mday = (int)day;
  tm->tm_yday = (int)yday;
  tm->tm_isdst = -1;
}

static struct tm static_tm;
static char asctime_buffer[ASCTIME_BUFFER_SIZE];

static void write_two_digits(char* dest, int value) {
  dest[0] = (char)('0' + (value / 10));
  dest[1] = (char)('0' + (value % 10));
}

static void write_two_digits_space(char* dest, int value) {
  if(value >= 10) {
    write_two_digits(dest, value);
  } else {
    dest[0] = ' ';
    dest[1] = (char)('0' + value);
  }
}

static void write_four_digits(char* dest, int value) {
  dest[0] = (char)('0' + ((value / 1000) % 10));
  dest[1] = (char)('0' + ((value / 100) % 10));
  dest[2] = (char)('0' + ((value / 10) % 10));
  dest[3] = (char)('0' + (value % 10));
}

static bool append_buffer(char* dest, size_t max, size_t* pos, const char* data, size_t len) {
  if(data == NULL) {
    return false;
  }
  if(len == (size_t)-1) {
    len = strlen(data);
  }
  if(*pos + len >= max) {
    return false;
  }
  memcpy(dest + *pos, data, len);
  *pos += len;
  dest[*pos] = '\0';
  return true;
}

static bool append_char(char* dest, size_t max, size_t* pos, char ch) {
  if(*pos + 1 >= max) {
    return false;
  }
  dest[*pos] = ch;
  (*pos)++;
  dest[*pos] = '\0';
  return true;
}

static bool append_two_digits(char* dest, size_t max, size_t* pos, int value) {
  char buf[2];
  if(value < 0 || value > 99) {
    return false;
  }
  write_two_digits(buf, value);
  return append_buffer(dest, max, pos, buf, sizeof(buf));
}

static bool append_three_digits(char* dest, size_t max, size_t* pos, int value) {
  if(value < 0 || value > 999) {
    return false;
  }
  char buf[3];
  buf[0] = (char)('0' + (value / 100));
  buf[1] = (char)('0' + ((value / 10) % 10));
  buf[2] = (char)('0' + (value % 10));
  return append_buffer(dest, max, pos, buf, sizeof(buf));
}

static bool append_four_digits(char* dest, size_t max, size_t* pos, int value) {
  if(value < 0) {
    return false;
  }
  char buf[4];
  write_four_digits(buf, value);
  return append_buffer(dest, max, pos, buf, sizeof(buf));
}

static bool append_signed_number(char* dest, size_t max, size_t* pos, long value) {
  char buf[32];
  size_t index = 0;
  unsigned long magnitude;

  if(value < 0) {
    if(!append_char(dest, max, pos, '-')) {
      return false;
    }
    magnitude = (unsigned long)(-value);
  } else {
    magnitude = (unsigned long)value;
  }

  do {
    buf[index++] = (char)('0' + (magnitude % 10));
    magnitude /= 10;
  } while(magnitude != 0 && index < sizeof(buf));

  if(magnitude != 0) {
    return false;
  }

  while(index > 0) {
    if(!append_char(dest, max, pos, buf[--index])) {
      return false;
    }
  }
  return true;
}

static bool timespec_valid(const struct timespec* ts) {
  if(ts == NULL) {
    return false;
  }
  if(ts->tv_sec < 0) {
    return false;
  }
  if(ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000L) {
    return false;
  }
  return true;
}

static bool timespec_to_microseconds(const struct timespec* ts, uint64_t* out_us) {
  if(!timespec_valid(ts) || out_us == NULL) {
    return false;
  }

  __uint128_t total_ns = (__uint128_t)(unsigned long long)ts->tv_sec * 1000000000ull +
                         (__uint128_t)(unsigned long long)ts->tv_nsec;
  __uint128_t total_us = (total_ns + 999u) / 1000u;
  if(total_us == 0) {
    total_us = (total_ns == 0) ? 0 : 1;
  }
  if(total_us > UINT64_MAX) {
    return false;
  }
  *out_us = (uint64_t)total_us;
  return true;
}

static void microseconds_to_timespec(uint64_t usec, struct timespec* ts) {
  if(ts == NULL) {
    return;
  }
  ts->tv_sec = (time_t)(usec / 1000000ull);
  uint64_t rem_us = usec % 1000000ull;
  ts->tv_nsec = (long)(rem_us * 1000ull);
}

static bool format_asctime_line(const struct tm* tm, char* buf, size_t size) {
  if(tm == NULL || buf == NULL || size < ASCTIME_BUFFER_SIZE) {
    return false;
  }

  if(tm->tm_wday < 0 || tm->tm_wday > 6 || tm->tm_mon < 0 || tm->tm_mon > 11) {
    return false;
  }

  int year = tm->tm_year + 1900;
  if(year < 0 || year > 9999 || tm->tm_mday < 1 || tm->tm_mday > 31 || tm->tm_hour < 0 || tm->tm_hour > 23 ||
     tm->tm_min < 0 || tm->tm_min > 59 || tm->tm_sec < 0 || tm->tm_sec > 60) {
    return false;
  }

  memcpy(buf, WEEKDAY_NAMES[tm->tm_wday], 3);
  buf[3] = ' ';
  memcpy(&buf[4], MONTH_NAMES[tm->tm_mon], 3);
  buf[7] = ' ';
  write_two_digits_space(&buf[8], tm->tm_mday);
  buf[10] = ' ';
  write_two_digits(&buf[11], tm->tm_hour);
  buf[13] = ':';
  write_two_digits(&buf[14], tm->tm_min);
  buf[16] = ':';
  write_two_digits(&buf[17], tm->tm_sec);
  buf[19] = ' ';
  write_four_digits(&buf[20], year);
  buf[24] = '\n';
  buf[25] = '\0';
  return true;
}

struct tm* gmtime_r(const time_t* timer, struct tm* result) {
  if(timer == NULL || result == NULL) {
#ifndef MENIOS_KERNEL
    errno = EINVAL;
#endif
    return NULL;
  }
  seconds_to_tm(*timer, result);
  return result;
}

struct tm* gmtime(const time_t* timer) {
  if(gmtime_r(timer, &static_tm) == NULL) {
    return NULL;
  }
  return &static_tm;
}

struct tm* localtime_r(const time_t* timer, struct tm* result) {
  return gmtime_r(timer, result);
}

struct tm* localtime(const time_t* timer) {
  if(localtime_r(timer, &static_tm) == NULL) {
    return NULL;
  }
  return &static_tm;
}

time_t mktime(struct tm* tm) {
  if(tm == NULL) {
#ifndef MENIOS_KERNEL
    errno = EINVAL;
#endif
    return (time_t)-1;
  }
  time_t seconds = tm_to_seconds(tm);
  if(seconds == (time_t)-1) {
#ifndef MENIOS_KERNEL
    errno = EINVAL;
#endif
    return (time_t)-1;
  }
  seconds_to_tm(seconds, tm);
  return seconds;
}

char* asctime_r(const struct tm* tm, char* buf) {
  if(!format_asctime_line(tm, buf, ASCTIME_BUFFER_SIZE)) {
#ifndef MENIOS_KERNEL
    errno = EINVAL;
#endif
    return NULL;
  }
  return buf;
}

char* asctime(const struct tm* tm) {
  if(asctime_r(tm, asctime_buffer) == NULL) {
    return NULL;
  }
  return asctime_buffer;
}

char* ctime(const time_t* timer) {
  if(timer == NULL) {
#ifndef MENIOS_KERNEL
    errno = EINVAL;
#endif
    return NULL;
  }
  if(localtime_r(timer, &static_tm) == NULL) {
    return NULL;
  }
  if(asctime_r(&static_tm, asctime_buffer) == NULL) {
    return NULL;
  }
  return asctime_buffer;
}

char* ctime_r(const time_t* timer, char* buf) {
  if(timer == NULL || buf == NULL) {
#ifndef MENIOS_KERNEL
    errno = EINVAL;
#endif
    return NULL;
  }
  struct tm tm_storage;
  if(localtime_r(timer, &tm_storage) == NULL) {
    return NULL;
  }
  return asctime_r(&tm_storage, buf);
}

double difftime(time_t end, time_t beginning) {
  return (double)end - (double)beginning;
}

size_t strftime(char* restrict dest,
                size_t max,
                const char* restrict format,
                const struct tm* restrict tm) {
  if(dest == NULL || format == NULL || tm == NULL || max == 0) {
    if(dest != NULL && max > 0) {
      dest[0] = '\0';
    }
    return 0;
  }

  size_t pos = 0;
  dest[0] = '\0';

  while(*format != '\0') {
    char ch = *format++;
    if(ch != '%') {
      if(!append_char(dest, max, &pos, ch)) {
        dest[0] = '\0';
        return 0;
      }
      continue;
    }

    char spec = *format++;
    if(spec == '\0') {
      dest[0] = '\0';
      return 0;
    }

    switch(spec) {
      case '%':
        if(!append_char(dest, max, &pos, '%')) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'a':
      case 'A': {
        int wday = tm->tm_wday;
        if(wday < 0 || wday > 6) {
          dest[0] = '\0';
          return 0;
        }
        const char* name = (spec == 'a') ? WEEKDAY_NAMES[wday] : WEEKDAY_FULL_NAMES[wday];
        if(!append_buffer(dest, max, &pos, name, (size_t)-1)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      }
      case 'b':
      case 'B': {
        int month = tm->tm_mon;
        if(month < 0 || month > 11) {
          dest[0] = '\0';
          return 0;
        }
        const char* name = (spec == 'b') ? MONTH_NAMES[month] : MONTH_FULL_NAMES[month];
        if(!append_buffer(dest, max, &pos, name, (size_t)-1)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      }
      case 'd':
        if(!append_two_digits(dest, max, &pos, tm->tm_mday)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'e': {
        int value = tm->tm_mday;
        if(value < 0 || value > 31) {
          dest[0] = '\0';
          return 0;
        }
        char buf[2];
        write_two_digits_space(buf, value);
        if(!append_buffer(dest, max, &pos, buf, sizeof(buf))) {
          dest[0] = '\0';
          return 0;
        }
        break;
      }
      case 'H':
        if(!append_two_digits(dest, max, &pos, tm->tm_hour)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'I': {
        int hour = tm->tm_hour;
        if(hour < 0 || hour > 23) {
          dest[0] = '\0';
          return 0;
        }
        int hour12 = hour % 12;
        if(hour12 == 0) {
          hour12 = 12;
        }
        if(!append_two_digits(dest, max, &pos, hour12)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      }
      case 'j':
        if(!append_three_digits(dest, max, &pos, tm->tm_yday + 1)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'm':
        if(!append_two_digits(dest, max, &pos, tm->tm_mon + 1)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'M':
        if(!append_two_digits(dest, max, &pos, tm->tm_min)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'n':
        if(!append_char(dest, max, &pos, '\n')) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'p': {
        int hour = tm->tm_hour;
        if(hour < 0 || hour > 23) {
          dest[0] = '\0';
          return 0;
        }
        const char* ampm = AM_PM_STRINGS[(hour >= 12) ? 1 : 0];
        if(!append_buffer(dest, max, &pos, ampm, 2)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      }
      case 'R':
        if(strftime(dest + pos, max - pos, "%H:%M", tm) == 0) {
          dest[0] = '\0';
          return 0;
        }
        pos = strlen(dest);
        break;
      case 'S':
        if(!append_two_digits(dest, max, &pos, tm->tm_sec)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 't':
        if(!append_char(dest, max, &pos, '\t')) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'T':
        if(strftime(dest + pos, max - pos, "%H:%M:%S", tm) == 0) {
          dest[0] = '\0';
          return 0;
        }
        pos = strlen(dest);
        break;
      case 'y': {
        int year = tm->tm_year % 100;
        if(year < 0) {
          year += 100;
        }
        if(!append_two_digits(dest, max, &pos, year)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      }
      case 'Y':
        if(!append_signed_number(dest, max, &pos, (long)tm->tm_year + 1900L)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'F':
        if(strftime(dest + pos, max - pos, "%Y-%m-%d", tm) == 0) {
          dest[0] = '\0';
          return 0;
        }
        pos = strlen(dest);
        break;
      case 'z':
        if(!append_buffer(dest, max, &pos, "+0000", 5)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      case 'Z':
        if(!append_buffer(dest, max, &pos, "UTC", 3)) {
          dest[0] = '\0';
          return 0;
        }
        break;
      default:
        dest[0] = '\0';
        return 0;
    }
  }

  return pos;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
  if(req == NULL) {
#ifndef MENIOS_KERNEL
    errno = EINVAL;
#endif
    return -1;
  }

#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall2(SYS_NANOSLEEP, (long)req, (long)rem);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return -1;
  }
  return 0;
#else
  typedef int (*host_nanosleep_fn)(const struct timespec*, struct timespec*);
  static host_nanosleep_fn real_nanosleep = NULL;
  if(real_nanosleep == NULL) {
    real_nanosleep = (host_nanosleep_fn)resolve_host_symbol("nanosleep");
    if(real_nanosleep == NULL) {
#ifndef MENIOS_KERNEL
      errno = ENOSYS;
#endif
      return -1;
    }
  }
  return real_nanosleep(req, rem);
#endif
}

unsigned int sleep(unsigned int seconds) {
  uint64_t total_us = (uint64_t)seconds * 1000000ull;
  struct timespec req;
  microseconds_to_timespec(total_us, &req);

  struct timespec rem;

  if(nanosleep(&req, &rem) == 0) {
    return 0;
  }

#ifndef MENIOS_KERNEL
  if(errno == EINTR) {
    uint64_t rem_us;
    if(timespec_to_microseconds(&rem, &rem_us)) {
      unsigned int left = (unsigned int)(rem_us / 1000000ull);
      if((rem_us % 1000000ull) != 0 && left < UINT_MAX) {
        left++;
      }
      return left;
    }
  }
#endif
  return seconds;
}

unsigned int alarm(unsigned int seconds) {
#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall1(SYS_ALARM, (long)seconds);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return 0;
  }
  return (unsigned int)rc;
#else
  typedef unsigned int (*host_alarm_fn)(unsigned int);
  static host_alarm_fn real_alarm = NULL;
  if(real_alarm == NULL) {
    real_alarm = (host_alarm_fn)resolve_host_symbol("alarm");
    if(real_alarm == NULL) {
#ifndef MENIOS_KERNEL
      errno = ENOSYS;
#endif
      return 0;
    }
  }
  return real_alarm(seconds);
#endif
}

int usleep(useconds_t usec) {
  struct timespec req;
  microseconds_to_timespec((uint64_t)usec, &req);
  return nanosleep(&req, NULL);
}

int clock_gettime(clockid_t clk_id, struct timespec* tp) {
  if(tp == NULL) {
#ifndef MENIOS_KERNEL
    errno = EFAULT;
#endif
    return -1;
  }

#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall2(SYS_CLOCK_GETTIME, (long)clk_id, (long)tp);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return -1;
  }
  return 0;
#else
  typedef int (*host_clock_gettime_fn)(clockid_t, struct timespec*);
  static host_clock_gettime_fn real_clock_gettime = NULL;
  if(real_clock_gettime == NULL) {
    real_clock_gettime = (host_clock_gettime_fn)resolve_host_symbol("clock_gettime");
    if(real_clock_gettime == NULL) {
#ifndef MENIOS_KERNEL
      errno = ENOSYS;
#endif
      return -1;
    }
  }
  return real_clock_gettime(clk_id, tp);
#endif
}

int clock_settime(clockid_t clk_id, const struct timespec* tp) {
  if(tp == NULL) {
#ifndef MENIOS_KERNEL
    errno = EFAULT;
#endif
    return -1;
  }

#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall2(SYS_CLOCK_SETTIME, (long)clk_id, (long)tp);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return -1;
  }
  return 0;
#else
  typedef int (*host_clock_settime_fn)(clockid_t, const struct timespec*);
  static host_clock_settime_fn real_clock_settime = NULL;
  if(real_clock_settime == NULL) {
    real_clock_settime = (host_clock_settime_fn)resolve_host_symbol("clock_settime");
    if(real_clock_settime == NULL) {
#ifndef MENIOS_KERNEL
      errno = ENOSYS;
#endif
      return -1;
    }
  }
  return real_clock_settime(clk_id, tp);
#endif
}

int clock_getres(clockid_t clk_id, struct timespec* res) {
  if(res == NULL) {
#ifndef MENIOS_KERNEL
    errno = EFAULT;
#endif
    return -1;
  }

#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall2(SYS_CLOCK_GETRES, (long)clk_id, (long)res);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return -1;
  }
  return 0;
#else
  typedef int (*host_clock_getres_fn)(clockid_t, struct timespec*);
  static host_clock_getres_fn real_clock_getres = NULL;
  if(real_clock_getres == NULL) {
    real_clock_getres = (host_clock_getres_fn)resolve_host_symbol("clock_getres");
    if(real_clock_getres == NULL) {
#ifndef MENIOS_KERNEL
      errno = ENOSYS;
#endif
      return -1;
    }
  }
  return real_clock_getres(clk_id, res);
#endif
}

int setitimer(int which, const struct itimerval* new_value, struct itimerval* old_value) {
  if(new_value == NULL) {
#ifndef MENIOS_KERNEL
    errno = EFAULT;
#endif
    return -1;
  }

#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall3(SYS_SETITIMER, (long)which, (long)new_value, (long)old_value);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return -1;
  }
  return 0;
#else
  typedef int (*host_setitimer_fn)(int, const struct itimerval*, struct itimerval*);
  static host_setitimer_fn real_setitimer = NULL;
  if(real_setitimer == NULL) {
    real_setitimer = (host_setitimer_fn)resolve_host_symbol("setitimer");
    if(real_setitimer == NULL) {
#ifndef MENIOS_KERNEL
      errno = ENOSYS;
#endif
      return -1;
    }
  }
  return real_setitimer(which, new_value, old_value);
#endif
}

int getitimer(int which, struct itimerval* value) {
  if(value == NULL) {
#ifndef MENIOS_KERNEL
    errno = EFAULT;
#endif
    return -1;
  }

#ifndef MENIOS_HOST_TEST
  long rc = __menios_syscall2(SYS_GETITIMER, (long)which, (long)value);
  if(rc < 0) {
#ifndef MENIOS_KERNEL
    errno = (int)(-rc);
#endif
    return -1;
  }
  return 0;
#else
  typedef int (*host_getitimer_fn)(int, struct itimerval*);
  static host_getitimer_fn real_getitimer = NULL;
  if(real_getitimer == NULL) {
    real_getitimer = (host_getitimer_fn)resolve_host_symbol("getitimer");
    if(real_getitimer == NULL) {
#ifndef MENIOS_KERNEL
      errno = ENOSYS;
#endif
      return -1;
    }
  }
  return real_getitimer(which, value);
#endif
}
