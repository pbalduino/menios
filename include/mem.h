#ifndef _KERNEL_MEM_H_
#define _KERNEL_MEM_H_ 1

#include <types.h>

void memzero(void * s, uint64_t n);
void*	memset(void *dst, int32_t c, size_t len);
void*	memcpy(void *dst, const void *src, size_t len);
void*	memmove(void *dst, const void *src, size_t len);
int	memcmp(const void *s1, const void *s2, size_t len);
void*	memfind(const void *s, int32_t c, size_t len);


#endif
