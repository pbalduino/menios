#ifndef MENIOS_INCLUDE_SYS_TIME_H
#define MENIOS_INCLUDE_SYS_TIME_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long suseconds_t;

#ifndef __timeval_defined
#define __timeval_defined 1
struct timeval {
  time_t      tv_sec;
  suseconds_t tv_usec;
};
#endif

struct timezone {
  int tz_minuteswest;
  int tz_dsttime;
};

struct itimerval {
  struct timeval it_interval;
  struct timeval it_value;
};

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2

int gettimeofday(struct timeval* tv, struct timezone* tz);
int setitimer(int which, const struct itimerval* new_value, struct itimerval* old_value);
int getitimer(int which, struct itimerval* value);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_SYS_TIME_H */
