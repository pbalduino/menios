#include "unity.h"

#include <errno.h>
#include <string.h>
#include <time.h>

void setUp(void) {}
void tearDown(void) {}

static void assert_tm(int year, int mon, int mday, int hour, int min, int sec, const struct tm* tm) {
  TEST_ASSERT_EQUAL_INT(sec, tm->tm_sec);
  TEST_ASSERT_EQUAL_INT(min, tm->tm_min);
  TEST_ASSERT_EQUAL_INT(hour, tm->tm_hour);
  TEST_ASSERT_EQUAL_INT(mday, tm->tm_mday);
  TEST_ASSERT_EQUAL_INT(mon, tm->tm_mon);
  TEST_ASSERT_EQUAL_INT(year, tm->tm_year);
}

void test_gmtime_conversion(void) {
  time_t t = 0; // 1970-01-01 00:00:00
  struct tm* tm = gmtime(&t);
  assert_tm(70, 0, 1, 0, 0, 0, tm);
  TEST_ASSERT_EQUAL_INT(4, tm->tm_wday);

  t = 946684800; // 2000-01-01 00:00:00 UTC
  tm = gmtime(&t);
  assert_tm(100, 0, 1, 0, 0, 0, tm);
  TEST_ASSERT_EQUAL_INT(6, tm->tm_wday);
  TEST_ASSERT_EQUAL_INT(0, tm->tm_yday);

  t = 1609459200; // 2021-01-01 00:00:00
  tm = gmtime(&t);
  assert_tm(121, 0, 1, 0, 0, 0, tm);
  TEST_ASSERT_EQUAL_INT(5, tm->tm_wday);
  TEST_ASSERT_EQUAL_INT(0, tm->tm_yday);

  t = 1612137600; // 2021-02-01 00:00:00
  tm = gmtime(&t);
  assert_tm(121, 1, 1, 0, 0, 0, tm);
  TEST_ASSERT_EQUAL_INT(1, tm->tm_wday);
  TEST_ASSERT_EQUAL_INT(31, tm->tm_yday);
}

void test_mktime_roundtrip(void) {
  struct tm tm = {
    .tm_year = 121,
    .tm_mon = 1,
    .tm_mday = 1,
    .tm_hour = 0,
    .tm_min = 0,
    .tm_sec = 0,
    .tm_isdst = -1,
  };
  time_t t = mktime(&tm);
  TEST_ASSERT_EQUAL_INT64(1612137600, t);
  TEST_ASSERT_EQUAL_INT(1, tm.tm_wday);
  TEST_ASSERT_EQUAL_INT(31, tm.tm_yday);

  struct tm leap = {
    .tm_year = 104, // 2004
    .tm_mon = 1,
    .tm_mday = 29,
    .tm_hour = 12,
    .tm_min = 34,
    .tm_sec = 56,
    .tm_isdst = -1,
  };
  time_t leap_seconds = mktime(&leap);
  struct tm* back = gmtime(&leap_seconds);
  assert_tm(104, 1, 29, 12, 34, 56, back);
}

void test_ctime_format(void) {
  time_t t = 0;
  char* str = ctime(&t);
  TEST_ASSERT_EQUAL_STRING("Thu Jan  1 00:00:00 1970\n", str);
}

void test_gmtime_r_matches_gmtime(void) {
  time_t t = 1609459200; // 2021-01-01 00:00:00 UTC
  struct tm tm_result;
  struct tm* rc = gmtime_r(&t, &tm_result);
  TEST_ASSERT_NOT_NULL(rc);
  TEST_ASSERT_EQUAL_PTR(&tm_result, rc);
  TEST_ASSERT_EQUAL_INT(121, tm_result.tm_year);
  TEST_ASSERT_EQUAL_INT(0, tm_result.tm_mon);
  TEST_ASSERT_EQUAL_INT(1, tm_result.tm_mday);
  TEST_ASSERT_EQUAL_INT(5, tm_result.tm_wday);
}

void test_asctime_r_writes_expected_format(void) {
  struct tm tm = {
    .tm_year = 123, // 2023
    .tm_mon = 6,
    .tm_mday = 9,
    .tm_hour = 7,
    .tm_min = 45,
    .tm_sec = 12,
    .tm_wday = 0,
  };
  char buffer[26];
  char* rc = asctime_r(&tm, buffer);
  TEST_ASSERT_EQUAL_PTR(buffer, rc);
  TEST_ASSERT_EQUAL_STRING("Sun Jul  9 07:45:12 2023\n", buffer);
}

void test_ctime_r_returns_buffer(void) {
  time_t t = 946684800; // 2000-01-01 00:00:00 UTC
  char buffer[26];
  char* rc = ctime_r(&t, buffer);
  TEST_ASSERT_EQUAL_PTR(buffer, rc);
  TEST_ASSERT_EQUAL_STRING("Sat Jan  1 00:00:00 2000\n", buffer);
}

void test_difftime_basic(void) {
  time_t start = 100;
  time_t end = 250;
  double diff = difftime(end, start);
  TEST_ASSERT_TRUE_MESSAGE(diff == 150.0, "difftime should return exact second difference");
}

void test_strftime_formats(void) {
  struct tm tm = {
    .tm_year = 121,
    .tm_mon = 1,
    .tm_mday = 1,
    .tm_hour = 13,
    .tm_min = 5,
    .tm_sec = 9,
    .tm_wday = 1,
    .tm_yday = 31,
  };

  char buffer[64];
  size_t len = strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm);
  TEST_ASSERT_EQUAL_UINT64(strlen("Mon Feb 01 13:05:09 2021"), len);
  TEST_ASSERT_EQUAL_STRING("Mon Feb 01 13:05:09 2021", buffer);

  len = strftime(buffer, sizeof(buffer), "%F %T %p %z %Z", &tm);
  TEST_ASSERT_EQUAL_UINT64(strlen("2021-02-01 13:05:09 PM +0000 UTC"), len);
  TEST_ASSERT_EQUAL_STRING("2021-02-01 13:05:09 PM +0000 UTC", buffer);

  char small[3];
  len = strftime(small, sizeof(small), "%Y", &tm);
  TEST_ASSERT_EQUAL_UINT64(0, len);
}

void test_nanosleep_zero_duration(void) {
  struct timespec req = {0, 0};
  TEST_ASSERT_EQUAL_INT(0, nanosleep(&req, NULL));
}

void test_clock_gettime_realtime(void) {
  struct timespec ts;
  TEST_ASSERT_EQUAL_INT(0, clock_gettime(CLOCK_REALTIME, &ts));
  TEST_ASSERT_TRUE(ts.tv_sec >= 0);
  TEST_ASSERT_TRUE(ts.tv_nsec >= 0);
  TEST_ASSERT_TRUE(ts.tv_nsec < 1000000000L);
}

void test_clock_gettime_monotonic_monotonic_increases(void) {
  struct timespec first;
  struct timespec second;
  TEST_ASSERT_EQUAL_INT(0, clock_gettime(CLOCK_MONOTONIC, &first));
  TEST_ASSERT_EQUAL_INT(0, clock_gettime(CLOCK_MONOTONIC, &second));
  if(second.tv_sec == first.tv_sec) {
    TEST_ASSERT_TRUE(second.tv_nsec >= first.tv_nsec);
  } else {
    TEST_ASSERT_TRUE(second.tv_sec >= first.tv_sec);
  }
}

void test_clock_getres_reports_resolution(void) {
  struct timespec res;
  TEST_ASSERT_EQUAL_INT(0, clock_getres(CLOCK_REALTIME, &res));
  TEST_ASSERT_TRUE(res.tv_nsec > 0);
}

void test_clock_gettime_invalid_clock(void) {
  errno = 0;
  struct timespec ts;
  int rc = clock_gettime(12345, &ts);
  if(rc == 0) {
    return;
  }
  TEST_ASSERT_EQUAL_INT(-1, rc);

  if(errno != 0) {
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_gmtime_conversion);
  RUN_TEST(test_mktime_roundtrip);
  RUN_TEST(test_ctime_format);
  RUN_TEST(test_gmtime_r_matches_gmtime);
  RUN_TEST(test_asctime_r_writes_expected_format);
  RUN_TEST(test_ctime_r_returns_buffer);
  RUN_TEST(test_difftime_basic);
  RUN_TEST(test_strftime_formats);
  RUN_TEST(test_nanosleep_zero_duration);
  RUN_TEST(test_clock_gettime_realtime);
  RUN_TEST(test_clock_gettime_monotonic_monotonic_increases);
  RUN_TEST(test_clock_getres_reports_resolution);
  RUN_TEST(test_clock_gettime_invalid_clock);
  return UNITY_END();
}
