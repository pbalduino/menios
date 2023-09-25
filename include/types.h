#ifndef MENIOS_INCLUDE_TYPES_H
#define MENIOS_INCLUDE_TYPES_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void*) 0)
#endif

typedef _Bool bool;
enum { false, true };

typedef short int16_t;
typedef int   int32_t;
typedef long  int64_t;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

typedef uint64_t size_t;

typedef unsigned long uintptr_t;

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
