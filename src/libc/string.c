#include <assert.h>
#include <string.h>

size_t strlen(const char *s) {
  uint16_t len = 0;

  while(s[len++]);

  return len;
}

void swap(char* a, char* b) {
  char t = *b;
  *b = *a;
  *a = t;
}

int	strcmp(const char *s1, const char *s2) {
  for(; *s1==*s2 && *s1; s1++, s2++){ };
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void strrev(char str[], int32_t length) {
  int32_t start = 0;
  int32_t end = length -1;
  while (start < end) {
    swap(&str[start], &str[end]);
    start++;
    end--;
  }
}

char*	strncat(char *dst, const char *src, size_t size) {
	if (size != 0) {
		char *d = dst;
		const char *s = src;
		while (*d != 0) d++;
		do {
			if ((*d = *s++) == 0) break;
			d++;
		} while (--size != 0);
		*d = 0;
	}

  assert(dst[strlen(dst) - 1] == 0);

	return dst;
}

void* memset(void *v, int32_t c, size_t n) {
	if (n == 0)
		return v;

	if ((int)v % 4 == 0 && n % 4 == 0) {
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

void * memcpy(void *dst, const void *src, size_t n) {
	return memmove(dst, src, n);
}
