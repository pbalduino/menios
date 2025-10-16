#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <types.h>

void*	memcpy(void *dst, const void *src, size_t len);
void memzero(void * s, uint64_t n);
void*	memset(void *dst, int32_t c, size_t len);
void*	memsetl(void *dst, int64_t c, size_t len);
void*	memmove(void *dst, const void *src, size_t len);
int	memcmp(const void *s1, const void *s2, size_t len);
void* memchr(const void *s, int c, size_t len);
void*	memfind(const void *s, int32_t c, size_t len);

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
char* strchr(const char *s, int c);
char*	strfind(const char *s, char c);
char *strstr (const char *str_1, const char *str_2);
char* strrchr(const char* s, int c);

#ifdef __cplusplus
}
#endif

#endif
