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
