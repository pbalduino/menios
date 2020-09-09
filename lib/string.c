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

void strrev(char str[], int length) {
  int start = 0;
  int end = length -1;
  while (start < end) {
    swap(&str[start], &str[end]);
    start++;
    end--;
  }
}

void* memset(void *v, int c, size_t n) {
	if (n == 0)
		return v;

	if ((int)v%4 == 0 && n%4 == 0) {
		c &= 0xFF;
		c = (c<<24)|(c<<16)|(c<<8)|c;
		asm volatile("cld; rep stosl\n"
			:: "D" (v), "a" (c), "c" (n/4)
			: "cc", "memory");
	} else {
		asm volatile("cld; rep stosb\n"
			:: "D" (v), "a" (c), "c" (n)
			: "cc", "memory");
  }

	return v;
}

void * memmove(void *dst, const void *src, size_t n) {
	const char *s;
	char *d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("std; rep movsl\n"
				:: "D" (d-4), "S" (s-4), "c" (n/4) : "cc", "memory");
		else
			asm volatile("std; rep movsb\n"
				:: "D" (d-1), "S" (s-1), "c" (n) : "cc", "memory");
		// Some versions of GCC rely on DF being clear
		asm volatile("cld" ::: "cc");
	} else {
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("cld; rep movsl\n"
				:: "D" (d), "S" (s), "c" (n/4) : "cc", "memory");
		else
			asm volatile("cld; rep movsb\n"
				:: "D" (d), "S" (s), "c" (n) : "cc", "memory");
	}
	return dst;
}

void * memcpy(void *dst, const void *src, size_t n) {
	return memmove(dst, src, n);
}
