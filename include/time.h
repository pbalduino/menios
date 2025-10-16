#ifndef INCLUDE_TIME_H
#define INCLUDE_TIME_H

#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLOCKS_PER_SEC 1000000L

time_t time(time_t* tloc);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_TIME_H */
