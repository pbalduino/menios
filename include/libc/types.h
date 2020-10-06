#ifndef MENIOS_INCLUDE_TYPES_H
#define MENIOS_INCLUDE_TYPES_H

#ifndef NULL
#define NULL ((void*) 0)
#endif

typedef _Bool bool;
enum { false, true };

typedef int int32_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;

typedef unsigned int size_t;
typedef unsigned int uint32_t;

typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
typedef uint32_t physaddr_t;

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

#endif
