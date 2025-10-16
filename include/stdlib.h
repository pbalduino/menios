#ifndef INCLUDE_STDLIB_H
#define INCLUDE_STDLIB_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __dead2 __attribute__((__noreturn__))

#define RAND_MAX 0x7fffffff

char* itoa(int32_t num, char* str, int32_t base);
char* utoa(uint32_t num, char* str, int32_t base);

char* ltoa(int64_t num, char* str, int32_t base);
char* lutoa(uint64_t num, char* str, int32_t base);
char* lutoca(uint64_t num, char* str, int32_t base);

void* malloc(size_t size);
void  free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void* reallocarray(void* ptr, size_t nmemb, size_t size);
typedef struct {
  size_t arena_count;
  size_t arena_payload_bytes;
  size_t buddy_free_payload_bytes;
  size_t buddy_free_blocks;
  size_t direct_allocations;
  size_t direct_bytes;
  size_t double_free_attempts;
} menios_malloc_stats_t;

int   posix_memalign(void** memptr, size_t alignment, size_t size);
void* aligned_alloc(size_t alignment, size_t size);
void* memalign(size_t alignment, size_t size);
void* valloc(size_t size);
void* pvalloc(size_t size);
size_t malloc_usable_size(void* ptr);
int menios_malloc_stats(menios_malloc_stats_t* stats);

int rand(void);
void srand(unsigned int seed);

long strtol(const char* nptr, char** endptr, int base);

void exit(int) __dead2;
void abort(void) __dead2;

#ifdef __cplusplus
}
#endif

#endif
