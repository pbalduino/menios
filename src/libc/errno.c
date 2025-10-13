#if !defined(MENIOS_KERNEL)
#include <sys/errno.h>

static int errno_value = 0;

int* __menios_errno_location(void) {
  return &errno_value;
}
#endif
