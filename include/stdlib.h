#ifndef INCLUDE_STDLIB_H
#define INCLUDE_STDLIB_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __dead2 __attribute__((__noreturn__))

char* itoa(int32_t num, char* str, int32_t base);
char* utoa(uint32_t num, char* str, int32_t base);

char* ltoa(int64_t num, char* str, int32_t base);
char* lutoa(uint64_t num, char* str, int32_t base);
char* lutoca(uint64_t num, char* str, int32_t base);

void* malloc(size_t size);
void  free(void* ptr);

void *aligned_alloc( size_t alignment, size_t size );

void exit(int) __dead2;

char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);

#ifdef __cplusplus
}
#endif

#endif
