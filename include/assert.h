#ifndef MENIOS_INCLUDE_ASSERT_H
#define MENIOS_INCLUDE_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/console.h>

void _warn(const char*, int, const char*, ...);
void _panic(const char*, int, const char*, ...) __attribute__((noreturn));

#define warn(...) _warn(__FILE__, __LINE__, __VA_ARGS__)
#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)
#define debug() printf("%s(%d)\n", __FILE__, __LINE__)

#define assert(x)		\
	do { if(!(x)) panic("assertion failed: %s", #x); } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)	switch (x) case 0: case (x):

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* MENIOS_INCLUDE_ASSERT_H */
