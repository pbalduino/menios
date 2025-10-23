#ifndef MENIOS_INCLUDE_UTIME_H
#define MENIOS_INCLUDE_UTIME_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct utimbuf {
  time_t actime;
  time_t modtime;
};

int utime(const char* filename, const struct utimbuf* times);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_UTIME_H */
