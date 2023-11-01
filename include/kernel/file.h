#ifndef _KERNEL_FILE_H
#define _KERNEL_FILE_H 1

#include <types.h>

typedef struct __fd_t {
  bool used;
  int (*write)(int32_t);
  int (*read)();
  int (*close)();
} *file_descriptor_t;

file_descriptor_t fd_get(int fd);

void file_init();

#endif
