#ifndef MENIOS_DIRENT_H
#define MENIOS_DIRENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8

struct dirent {
  unsigned char d_type;
  char          d_name[256];
};

typedef struct DIR DIR;

DIR* opendir(const char* path);
int closedir(DIR* dir);
struct dirent* readdir(DIR* dir);
void rewinddir(DIR* dir);
long telldir(DIR* dir);
void seekdir(DIR* dir, long loc);

#ifdef __cplusplus
}
#endif

#endif
