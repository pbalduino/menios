#ifndef INCLUDE_STDLIB_H
#define INCLUDE_STDLIB_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

char* itoa(int32_t num, char* str, int32_t base);
char* utoa(uint32_t num, char* str, int32_t base);

char* ltoa(int64_t num, char* str, int32_t base);
char* lutoa(uint64_t num, char* str, int32_t base);

void* malloc(size_t size);
void  free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif
