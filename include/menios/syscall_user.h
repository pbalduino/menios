#ifndef MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H
#define MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MENIOS_HOST_TEST

#include <unistd.h>
#include <sys/syscall.h>

long syscall(long number, ...);

static inline long __menios_syscall0(long number) {
  return syscall(number);
}

static inline long __menios_syscall1(long number, long arg1) {
  return syscall(number, arg1);
}

static inline long __menios_syscall2(long number, long arg1, long arg2) {
  return syscall(number, arg1, arg2);
}

static inline long __menios_syscall3(long number, long arg1, long arg2, long arg3) {
  return syscall(number, arg1, arg2, arg3);
}

static inline long __menios_syscall4(long number, long arg1, long arg2, long arg3, long arg4) {
  return syscall(number, arg1, arg2, arg3, arg4);
}

static inline long __menios_syscall5(long number,
                                     long arg1,
                                     long arg2,
                                     long arg3,
                                     long arg4,
                                     long arg5) {
  return syscall(number, arg1, arg2, arg3, arg4, arg5);
}

static inline long __menios_syscall6(long number,
                                     long arg1,
                                     long arg2,
                                     long arg3,
                                     long arg4,
                                     long arg5,
                                     long arg6) {
  return syscall(number, arg1, arg2, arg3, arg4, arg5, arg6);
}

#else

static volatile long __attribute__((unused)) __menios_syscall_last_result;

#define MENIOS_ALWAYS_INLINE __attribute__((always_inline)) inline

static MENIOS_ALWAYS_INLINE long __menios_syscall0(long number) {
  long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(number)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = result;
  return result;
}

static MENIOS_ALWAYS_INLINE long __menios_syscall1(long number, long arg1) {
  long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(number), "D"(arg1)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = result;
  return result;
}

static MENIOS_ALWAYS_INLINE long __menios_syscall2(long number, long arg1, long arg2) {
  long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(number), "D"(arg1), "S"(arg2)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = result;
  return result;
}

static MENIOS_ALWAYS_INLINE long __menios_syscall3(long number,
                                                   long arg1,
                                                   long arg2,
                                                   long arg3) {
  long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = result;
  return result;
}

static MENIOS_ALWAYS_INLINE long __menios_syscall4(long number,
                                                   long arg1,
                                                   long arg2,
                                                   long arg3,
                                                   long arg4) {
  long result;
  asm volatile("mov %5, %%r10\n\t"
               "syscall"
               : "=a"(result)
               : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4)
               : "rcx", "r11", "r10", "memory");
  __menios_syscall_last_result = result;
  return result;
}

static MENIOS_ALWAYS_INLINE long __menios_syscall5(long number,
                                                   long arg1,
                                                   long arg2,
                                                   long arg3,
                                                   long arg4,
                                                   long arg5) {
  long result;
  asm volatile("mov %5, %%r10\n\t"
               "mov %6, %%r8\n\t"
               "syscall"
               : "=a"(result)
               : "a"(number),
                 "D"(arg1),
                 "S"(arg2),
                 "d"(arg3),
                 "r"(arg4),
                 "r"(arg5)
               : "rcx", "r11", "r10", "r8", "memory");
  __menios_syscall_last_result = result;
  return result;
}

static MENIOS_ALWAYS_INLINE long __menios_syscall6(long number,
                                                   long arg1,
                                                   long arg2,
                                                   long arg3,
                                                   long arg4,
                                                   long arg5,
                                                   long arg6) {
  long result;
  asm volatile("mov %5, %%r10\n\t"
               "mov %6, %%r8\n\t"
               "mov %7, %%r9\n\t"
               "syscall"
               : "=a"(result)
               : "a"(number),
                 "D"(arg1),
                 "S"(arg2),
                 "d"(arg3),
                 "r"(arg4),
                 "r"(arg5),
                 "r"(arg6)
               : "rcx", "r11", "r10", "r8", "r9", "memory");
  __menios_syscall_last_result = result;
  return result;
}

#undef MENIOS_ALWAYS_INLINE

#endif

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H
