#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t strlen(const char *s);

void strrev(char str[], int32_t length);

size_t strnlen(const char *s, size_t size);
char*	strcpy(char *dst, const char *src);
char*	strncpy(char *dst, const char *src, size_t size);

char*	strcat(char *dst, const char *src);
char*	strncat(char *dst, const char *src, size_t size);

size_t strlcpy(char *dst, const char *src, size_t size);
int32_t	strcmp(const char *s1, const char *s2);
int32_t	strncmp(const char *s1, const char *s2, size_t size);
char*	strchr(const char *s, char c);
char*	strfind(const char *s, char c);

long strtol(const char *s, char **endptr, int32_t base);

#ifdef __cplusplus
}
#endif

#endif
