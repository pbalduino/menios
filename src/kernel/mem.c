#include <stdint.h>
#include <stddef.h>

#include <stdint.h>
#include <stddef.h>

void* memcpy(void* dest, const void* src, size_t n) {
  uint64_t* dest64 = (uint64_t*)dest;
  const uint64_t* src64 = (const uint64_t*)src;
  size_t num_qwords = n / sizeof(uint64_t);
  size_t remaining_bytes = n % sizeof(uint64_t);

  // Copy as many 64-bit chunks as possible
  for (size_t i = 0; i < num_qwords; i++) {
    dest64[i] = src64[i];
  }

  // Copy the remaining bytes (if any)
  uint8_t* dest8 = (uint8_t*)&dest64[num_qwords];
  const uint8_t* src8 = (const uint8_t*)&src64[num_qwords];
  for (size_t i = 0; i < remaining_bytes; i++) {
    dest8[i] = src8[i];
  }

  return dest;
}

void* memset(void *v, int32_t c, size_t n) {
	if (n == 0)
		return v;

	if ((uintptr_t)v % 4 == 0 && n % 4 == 0) {
		c &= 0xff;
		c = (c << 24) | (c << 16) | (c << 8) | c;
		asm volatile("cld; rep stosl\n"
			:: "D" (v), "a" (c), "c" (n / 4)
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

void *memmove(void *dest, const void *src, size_t n) {
  char *d = dest;
  const char *s = src;

  if (d==s) return d;
  if ((uintptr_t)s-(uintptr_t)d-n <= -2*n) return memcpy(d, s, n);

  if (d<s) {
#ifdef __GNUC__
	if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
		while ((uintptr_t)d % WS) {
  		if (!n--) return dest;
  		*d++ = *s++;
		}
		for (; n>=WS; n-=WS, d+=WS, s+=WS) *(WT *)d = *(WT *)s;
	}
#endif
		for (; n; n--) *d++ = *s++;
	} else {
#ifdef __GNUC__
	if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
		while ((uintptr_t)(d+n) % WS) {
			if (!n--) return dest;
			d[n] = s[n];
		}
		while (n>=WS) n-=WS, *(WT *)(d+n) = *(WT *)(s+n);
	}
#endif
		while (n) n--, d[n] = s[n];
	}

	return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;

  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

  return 0;
}

void memzero(void* s, uint64_t n) {
  for (int i = n; i > 0; i--) ((uint8_t*)s)[i - 1] = 0;
}
