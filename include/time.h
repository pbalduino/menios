#ifndef INCLUDE_TIME_H
#define INCLUDE_TIME_H

#include <sys/types.h>
#include <sys/time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLOCKS_PER_SEC 1000000L

time_t time(time_t* tloc);

struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};

struct tm* gmtime(const time_t* timer);
struct tm* gmtime_r(const time_t* timer, struct tm* result);
struct tm* localtime(const time_t* timer);
struct tm* localtime_r(const time_t* timer, struct tm* result);
time_t mktime(struct tm* tm);
char* asctime(const struct tm* tm);
char* asctime_r(const struct tm* tm, char* buffer);
char* ctime(const time_t* timer);
char* ctime_r(const time_t* timer, char* buffer);
double difftime(time_t end, time_t beginning);
size_t strftime(char* restrict dest,
                size_t max,
                const char* restrict format,
                const struct tm* restrict tm);
int nanosleep(const struct timespec* req, struct timespec* rem);
unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_TIME_H */
