#include <assert.h>
#include <string.h>
#include <types.h>

size_t strlen(const char *s) {
  uint16_t len = 0;

  while(s[len++]);

  return len;
}

bool swap(char* a, char* b) {
  if (a == NULL || b == NULL) {
    return false;
  }

  char t = *a;
  *a = *b;
  *b = t;

  return true;
}

int	strcmp(const char *s1, const char *s2) {
  for(; *s1==*s2 && *s1; s1++, s2++){ };
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int	strncmp(const char *s1, const char *s2, size_t num) {
  for(; num && *s1==*s2 && *s1; s1++, s2++, num--){ 
    if (*s1 == '\0') {
      return 0;
    }
  };

  if (num == 0) {
    return 0;
  }

	return *(uint8_t*)s1 - *(uint8_t*)s2;
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

char* strcat(char* dst, const char* src) {
  char* ptr = dst;
  while (*ptr != '\0') {
    ptr++;
  }

  // Append the source string to the destination string
  while (*src != '\0') {
    *ptr = *src;
    ptr++;
    src++;
  }

  *ptr = '\0';

  return dst;
}

char*	strcpy(char *dst, const char *src) {
  char* original = dst;

  while (*src != '\0') {
    *dst = *src;
    dst++;
    src++;
  }

  *dst = '\0';

  return original;
}

char*	strncpy(char *dst, const char *src, size_t size) {
  size_t i;

  // Copy up to 'num' characters from source to destination
  for (i = 0; i < size && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }

  // If the length of the source is less than 'num', fill the rest with null characters
  for (; i < size; i++) {
    dst[i] = '\0';
  }

  return dst;
}

#ifdef __GNUC__
typedef __attribute__((__may_alias__)) size_t WT;
#define WS (sizeof(WT))
#endif
