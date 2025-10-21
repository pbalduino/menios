#include <assert.h>
#ifndef MENIOS_KERNEL
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#endif
#include <stdint.h>
#include <string.h>
#include <types.h>

#ifdef MENIOS_KERNEL
#include <kernel/serial.h>

static inline int memcpy_is_canonical(uintptr_t addr) {
  return ((addr >> 47) == 0ull) || ((addr >> 47) == 0x1ffffull);
}
#endif

/**
 * Calculates the length of a null-terminated string.
 *
 * This function computes the number of characters in the string pointed
 * to by 's', excluding the terminating null byte ('\0').
 *
 * @param s Pointer to the null-terminated string to be measured.
 * @return  The number of bytes in the string pointed to by 's'.
 *
 * Note: This function does not check for buffer overruns. Ensure that
 * the input is a valid null-terminated string to avoid undefined behavior.
 */
size_t strlen(const char* s) {
  uint16_t len = 0;

  while(s[len]) {
    len++;
  }

  return len;
}

/**
 * Calculates the length of a string, limited by a maximum length.
 *
 * This function computes the number of characters in the string pointed
 * to by 's', up to a maximum of 'maxlen' characters. It stops counting
 * when either the null terminator is encountered or 'maxlen' characters
 * have been examined, whichever comes first.
 *
 * @param s      Pointer to the string to be measured.
 * @param maxlen Maximum number of characters to examine.
 * @return       The number of characters in the string, not including the
 *               terminating null byte ('\0'), but at most maxlen.
 *
 * Note: If the null terminator is not found within the first 'maxlen'
 * characters, the function will return 'maxlen'.
 */
size_t strnlen(const char* s, size_t maxlen) {
  uint16_t len = 0;

  while(s[len] && len++ < maxlen);

  return len;
}

/**
 * Swaps the values of two characters
 * @param a Pointer to the first character
 * @param b Pointer to the second character
 * @return true if swap was successful, false if either pointer is NULL
 */
bool swap(char* a, char* b) {
  if(a == NULL || b == NULL) {
    return false;
  }

  char t = *a;
  *a = *b;
  *b = t;

  return true;
}

char* strstr(const char* haystack, const char* needle) {
  if(haystack == NULL || needle == NULL) {
    return NULL;
  }

  if(*needle == '\0') {
    return (char*)haystack;
  }

  size_t needle_len = strlen(needle);
  if(needle_len == 0) {
    return (char*)haystack;
  }

  for(const char* it = haystack; *it != '\0'; ++it) {
    if(*it != *needle) {
      continue;
    }

    size_t i = 0;
    while(i < needle_len && it[i] == needle[i]) {
      i++;
    }
    if(i == needle_len) {
      return (char*)it;
    }
  }

  return NULL;
}

/**
 * Compares two strings lexicographically
 * @param s1 Pointer to the first string
 * @param s2 Pointer to the second string
 * @return Integer less than, equal to, or greater than zero if s1 is found,
 *         respectively, to be less than, to match, or be greater than s2
 * @note The behavior is undefined if either s1 or s2 is NULL
 */
int	strcmp(const char *s1, const char *s2) {
  for(; *s1 == *s2 && *s1; s1++, s2++){ };
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/**
 * Compares up to num characters of two strings lexicographically
 * @param s1 Pointer to the first string
 * @param s2 Pointer to the second string
 * @param num Maximum number of characters to compare
 * @return Integer less than, equal to, or greater than zero if s1 is found,
 *         respectively, to be less than, to match, or be greater than s2
 * @note The behavior is undefined if either s1 or s2 is NULL
 */
int	strncmp(const char *s1, const char *s2, size_t num) {
  for(; num && *s1 == *s2 && *s1; s1++, s2++, num--){
    if(*s1 == '\0') {
      return 0;
    }
  };

  if(num == 0) {
    return 0;
  }

	return *(uint8_t*)s1 - *(uint8_t*)s2;
}

/**
 * Reverses a string in place
 * @param str Array containing the string to be reversed
 * @param length Length of the string
 * @note The behavior is undefined if str is NULL or length is incorrect
 */
void strrev(char str[], int32_t length) {
  int32_t start = 0;
  int32_t end = length -1;
  while(start < end) {
    swap(&str[start], &str[end]);
    start++;
    end--;
  }
}

/**
 * Concatenates n characters from source string to destination string
 * @param dst Pointer to the destination string
 * @param src Pointer to the source string
 * @param size Maximum number of characters to concatenate
 * @return Pointer to the destination string
 * @note The behavior is undefined if either dst or src is NULL
 * @note Ensures null-termination of the resulting string
 */
char*	strncat(char *dst, const char *src, size_t size) {
	if(size != 0) {
		char *d = dst;
		const char *s = src;
		while(*d != 0) d++;
		do {
			if((*d = *s++) == 0) break;
			d++;
		} while(--size != 0);
		*d = 0;
	}

	return dst;
}

/**
 * Concatenates source string to destination string
 * @param dst Pointer to the destination string
 * @param src Pointer to the source string
 * @return Pointer to the destination string
 * @note The behavior is undefined if either dst or src is NULL
 * @note Ensures null-termination of the resulting string
 */
char* strcat(char* dst, const char* src) {
  char* ptr = dst;
  while(*ptr != '\0') {
    ptr++;
  }

  // Append the source string to the destination string
  while(*src != '\0') {
    *ptr = *src;
    ptr++;
    src++;
  }

  *ptr = '\0';

  return dst;
}

/**
 * Copies source string to destination string
 * @param dst Pointer to the destination buffer
 * @param src Pointer to the source string
 * @return Pointer to the destination string
 * @note The behavior is undefined if either dst or src is NULL
 * @note The destination buffer must be large enough to contain the source string
 */
char*	strcpy(char *dst, const char *src) {
  char* original = dst;

  while(*src != '\0') {
    *dst = *src;
    dst++;
    src++;
  }

  *dst = '\0';

  return original;
}

char* strchr(const char* s, int c) {
  if(s == NULL) {
    return NULL;
  }

  unsigned char target = (unsigned char)c;
  while(*s != '\0') {
    if((unsigned char)*s == target) {
      return (char*)s;
    }
    s++;
  }

  if(target == '\0') {
    return (char*)s;
  }

  return NULL;
}

char* strrchr(const char* s, int c) {
  if(s == NULL) {
    return NULL;
  }

  const char* last = NULL;
  unsigned char target = (unsigned char)c;

  while(*s != '\0') {
    if((unsigned char)*s == target) {
      last = s;
    }
    s++;
  }

  if(target == '\0') {
    return (char*)s;
  }

  return (char*)last;
}

#ifndef MENIOS_KERNEL

char* strdup(const char* s) {
  if(s == NULL) {
    return NULL;
  }

  size_t len = strlen(s);
  char* copy = (char*)malloc(len + 1);
  if(copy == NULL) {
    errno = ENOMEM;
    return NULL;
  }

  memcpy(copy, s, len + 1);
  return copy;
}

char* strndup(const char* s, size_t n) {
  if(s == NULL) {
    return NULL;
  }

  size_t len = strnlen(s, n);
  char* copy = (char*)malloc(len + 1);
  if(copy == NULL) {
    errno = ENOMEM;
    return NULL;
  }

  memcpy(copy, s, len);
  copy[len] = '\0';
  return copy;
}

int strcasecmp(const char* s1, const char* s2) {
  if(s1 == NULL && s2 == NULL) {
    return 0;
  }
  if(s1 == NULL) {
    return -1;
  }
  if(s2 == NULL) {
    return 1;
  }

  while(*s1 != '\0' && *s2 != '\0') {
    unsigned char c1 = (unsigned char)tolower((unsigned char)*s1);
    unsigned char c2 = (unsigned char)tolower((unsigned char)*s2);

    if(c1 != c2) {
      return (int)c1 - (int)c2;
    }

    s1++;
    s2++;
  }

  return (int)(unsigned char)tolower((unsigned char)*s1) -
         (int)(unsigned char)tolower((unsigned char)*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
  if(n == 0) {
    return 0;
  }

  if(s1 == NULL && s2 == NULL) {
    return 0;
  }
  if(s1 == NULL) {
    return -1;
  }
  if(s2 == NULL) {
    return 1;
  }

  while(n-- > 0) {
    unsigned char c1 = (unsigned char)tolower((unsigned char)*s1++);
    unsigned char c2 = (unsigned char)tolower((unsigned char)*s2++);

    if(c1 != c2) {
      return (int)c1 - (int)c2;
    }

    if(c1 == '\0') {
      return 0;
    }
  }

  return 0;
}

#endif /* !MENIOS_KERNEL */

/**
 * Copies up to size characters from source string to destination buffer
 * @param dst Pointer to the destination buffer
 * @param src Pointer to the source string
 * @param size Maximum number of characters to copy
 * @return Pointer to the destination string
 * @note The behavior is undefined if either dst or src is NULL
 * @note If src is less than size characters, remaining space is filled with nulls
 */
char*	strncpy(char *dst, const char *src, size_t size) {
  size_t i;

  // Copy up to 'num' characters from source to destination
  for(i = 0; i < size && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }

  // If the length of the source is less than 'num', fill the rest with null characters
  for(; i < size; i++) {
    dst[i] = '\0';
  }

  return dst;
}

#ifdef __GNUC__
typedef __attribute__((__may_alias__)) size_t WT;
#define WS (sizeof(WT))
#endif

void* memset(void* dest, int value, size_t count) {
  unsigned char* d = (unsigned char*)dest;
  unsigned char byte = (unsigned char)value;

#ifdef __GNUC__
  WT pattern = 0;
  if(count >= WS) {
    for(size_t i = 0; i < WS; i++) {
      pattern <<= 8;
      pattern |= (WT)byte;
    }

    while(((uintptr_t)d & (WS - 1)) != 0 && count > 0) {
      *d++ = byte;
      count--;
    }

    WT* dw = (WT*)d;
    while(count >= WS) {
      *dw++ = pattern;
      count -= WS;
    }
    d = (unsigned char*)dw;
  }
#endif

  while(count-- > 0) {
    *d++ = byte;
  }

  return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
  unsigned char* d = (unsigned char*)dest;
  const unsigned char* s = (const unsigned char*)src;

#ifdef MENIOS_KERNEL
  const uintptr_t dest_addr = (uintptr_t)d;
  const uintptr_t src_addr = (uintptr_t)s;
  const uintptr_t kernel_floor = 0xffff800000000000ull;
  if(count != 0 &&
     (!memcpy_is_canonical(dest_addr) || !memcpy_is_canonical(src_addr))) {
    serial_printf("memcpy guard canonical: dest=%p src=%p len=%zu ra0=%p\n",
                  dest,
                  src,
                  count,
                  __builtin_return_address(0));
    return dest;
  }

  if(count != 0 && (dest_addr < kernel_floor || src_addr < kernel_floor)) {
    serial_printf("memcpy low addr: dest=%p src=%p len=%zu ra0=%p\n",
                  dest,
                  src,
                  count,
                  __builtin_return_address(0));
  }
#endif

#ifdef __GNUC__
  if(count >= WS) {
    while(((uintptr_t)d & (WS - 1)) != 0 && count > 0) {
      *d++ = *s++;
      count--;
    }

    if(((uintptr_t)s & (WS - 1)) == 0) {
      WT* dw = (WT*)d;
      const WT* sw = (const WT*)s;
      while(count >= WS) {
        *dw++ = *sw++;
        count -= WS;
      }
      d = (unsigned char*)dw;
      s = (const unsigned char*)sw;
    }
  }
#endif

  while(count-- > 0) {
    *d++ = *s++;
  }

  return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
  unsigned char* d = (unsigned char*)dest;
  const unsigned char* s = (const unsigned char*)src;

  if(d == s || count == 0) {
    return dest;
  }

  if(d < s) {
    return memcpy(dest, src, count);
  }

  d += count;
  s += count;
  while(count-- > 0) {
    *--d = *--s;
  }

  return dest;
}

int memcmp(const void* lhs, const void* rhs, size_t count) {
  const unsigned char* a = (const unsigned char*)lhs;
  const unsigned char* b = (const unsigned char*)rhs;

  while(count-- > 0) {
    if(*a != *b) {
      return (int)(*a - *b);
    }
    a++;
    b++;
  }

  return 0;
}

void* memchr(const void* ptr, int value, size_t count) {
  const unsigned char* p = (const unsigned char*)ptr;
  unsigned char target = (unsigned char)value;

  while(count-- > 0) {
    if(*p == target) {
      return (void*)p;
    }
    p++;
  }

  return NULL;
}
