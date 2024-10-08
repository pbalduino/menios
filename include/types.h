#ifndef MENIOS_INCLUDE_TYPES_H
#define MENIOS_INCLUDE_TYPES_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef long long off_t;

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

typedef uintptr_t phys_addr_t;
typedef uintptr_t virt_addr_t;

#ifdef __cplusplus
}
#endif

#endif
