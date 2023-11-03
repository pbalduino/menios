#include <assert.h>
#include <mem.h>
#include <string.h>
#include <types.h>

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

#ifdef __GNUC__
typedef __attribute__((__may_alias__)) size_t WT;
#define WS (sizeof(WT))
#endif
