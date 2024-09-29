#ifndef MENIOS_INCLUDE_SETJMP_H
#define MENIOS_INCLUDE_SETJMP_H

#include <stdint.h>
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t jmp_buf[16];

#ifdef __GNUC__
#define setjmp(buf)       __builtin_setjmp (buf)
#define longjmp(buf, val) __builtin_longjmp(buf, val)
#else
extern int setjmp(jmp_buf environment);
extern void longjmp(jmp_buf environment, int value) __attribute__((__noreturn__));
#endif

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_SETJMP_H */