#if !defined(MENIOS_KERNEL)
#include <sys/errno.h>

#if defined(MENIOS_HOST_TEST)
#if defined(__linux__)
extern int* __errno_location(void);
#elif defined(__APPLE__)
extern int* __error(void);
#endif

int* __menios_errno_location(void) {
#if defined(__linux__)
  return __errno_location();
#elif defined(__APPLE__)
  return __error();
#else
  static int errno_value = 0;
  return &errno_value;
#endif
}

#else /* !MENIOS_HOST_TEST */

static int errno_value = 0;

int* __menios_errno_location(void) {
  return &errno_value;
}

#endif /* MENIOS_HOST_TEST */

#endif /* !MENIOS_KERNEL */
