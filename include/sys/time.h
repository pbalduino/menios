#ifndef MENIOS_INCLUDE_SYS_TIME_H
#define MENIOS_INCLUDE_SYS_TIME_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long suseconds_t;

struct timeval {
  time_t      tv_sec;
  suseconds_t tv_usec;
};

struct timezone {
  int tz_minuteswest;
  int tz_dsttime;
};

int gettimeofday(struct timeval* tv, struct timezone* tz);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_SYS_TIME_H */
