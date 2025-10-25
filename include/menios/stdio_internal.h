#ifndef MENIOS_STDIO_INTERNAL_H
#define MENIOS_STDIO_INTERNAL_H

#include <stddef.h>
#include <sys/types.h>

struct _IO_FILE {
  int           fd;
  unsigned int  flags;
  unsigned char*buffer;
  size_t        buffer_size;
  size_t        buffer_pos;
  size_t        buffer_end;
  off_t         offset;
  int           error_number;
  int           last_op;
};

#endif /* MENIOS_STDIO_INTERNAL_H */
