#ifndef MENIOS_INCLUDE_TYPES_H
#define MENIOS_INCLUDE_TYPES_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef _Bool bool;
enum { false, true };

#define ROUNDDOWN(a, n)						\
({								\
	uint32_t __a = (uint32_t) (a);				\
	(typeof(a)) (__a - __a % (n));				\
})
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)						\
({								\
	uint32_t __n = (uint32_t) (n);				\
	(typeof(a)) (ROUNDDOWN((uint32_t) (a) + __n - 1, __n));	\
})

#ifdef __cplusplus
}
#endif

#endif
