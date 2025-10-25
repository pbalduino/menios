#ifndef INCLUDE_STDLIB_H
#define INCLUDE_STDLIB_H

#include <types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __dead2 __attribute__((__noreturn__))

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

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
long long strtoll(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
unsigned long long strtoull(const char* nptr, char** endptr, int base);
int atoi(const char* nptr);
long atol(const char* nptr);
double atof(const char* nptr);
double strtod(const char* nptr, char** endptr);
float strtof(const char* nptr, char** endptr);
#define strtold strtod
size_t mbstowcs(wchar_t* dest, const char* src, size_t max);
size_t wcstombs(char* dest, const wchar_t* src, size_t max);
int abs(int value);
long labs(long value);
long long llabs(long long value);
int system(const char* command);
char* mktemp(char* templ);
int mkstemp(char* templ);
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));

void exit(int) __dead2;
void abort(void) __dead2;
int atexit(void (*func)(void));
char* realpath(const char* path, char* resolved_path);

extern char** environ;
char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int putenv(char* string);
int unsetenv(const char* name);
int clearenv(void);

#ifdef __cplusplus
}
#endif

#endif
