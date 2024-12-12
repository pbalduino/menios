#include <assert.h>
#include <string.h>
#include <types.h>

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
