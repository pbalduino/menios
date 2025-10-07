/**
 * mem_utils.c - Memory Manager Utils
 * Contains functions to manipulate contiguous memory areas like arrays
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

void* memsetl(void *v, int64_t c, size_t n) {
	if(n == 0)
		return v;

	if((uintptr_t)v % 8 == 0 && n % 8 == 0) {
		asm volatile("cld; rep stosq\n"
			:
			: "D" (v), "a" (c), "c" (n)
			: "cc", "memory");
	} else {
		asm volatile("cld; rep stosb\n"
			:: "D" (v), "a" (c), "c" (n)
			: "cc", "memory");
  }

	return v;
}

#ifdef __GNUC__
typedef __attribute__((__may_alias__)) size_t WT;
#define WS (sizeof(WT))
#endif

void memzero(void* s, uint64_t n) {
	memset(s, 0, n);
}
