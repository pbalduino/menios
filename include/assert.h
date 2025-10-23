#ifndef MENIOS_INCLUDE_ASSERT_H
#define MENIOS_INCLUDE_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MENIOS_KERNEL

#include <kernel/console.h>

void _warn(const char*, int, const char*, ...);
void _panic(const char*, int, const char*, ...) __attribute__((noreturn));

#define warn(...) _warn(__FILE__, __LINE__, __VA_ARGS__)
#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)
#define debug() printf("%s(%d)\n", __FILE__, __LINE__)

#  ifdef NDEBUG
#    define assert(expr) ((void)0)
#  else
#    define assert(expr) ((expr) ? (void)0 : panic("assertion failed: %s", #expr))
#  endif

#else

#include <stddef.h>

void __menios_assert_fail(const char* expr,
                          const char* file,
                          int line,
                          const char* func) __attribute__((noreturn));

#  ifdef NDEBUG
#    define assert(expr) ((void)0)
#  else
#    define assert(expr) \
      ((expr) ? (void)0 : __menios_assert_fail(#expr, __FILE__, __LINE__, __func__))
#  endif

#endif

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x) switch (x) case 0: case (x):

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* MENIOS_INCLUDE_ASSERT_H */
