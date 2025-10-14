#ifndef MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H
#define MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MENIOS_HOST_TEST

#include <unistd.h>
#include <sys/syscall.h>

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

static inline long __menios_syscall0(long number) {
  register long rax asm("rax") = number;
  asm volatile("syscall"
               : "+a"(rax)
               :
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = rax;
  return rax;
}

static inline long __menios_syscall1(long number, long arg1) {
  register long rax asm("rax") = number;
  asm volatile("syscall"
               : "+a"(rax)
               : "D"(arg1)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = rax;
  return rax;
}

static inline long __menios_syscall2(long number, long arg1, long arg2) {
  register long rax asm("rax") = number;
  asm volatile("syscall"
               : "+a"(rax)
               : "D"(arg1), "S"(arg2)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = rax;
  return rax;
}

static inline long __menios_syscall3(long number, long arg1, long arg2, long arg3) {
  register long rax asm("rax") = number;
  asm volatile("syscall"
               : "+a"(rax)
               : "D"(arg1), "S"(arg2), "d"(arg3)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = rax;
  return rax;
}

static inline long __menios_syscall4(long number, long arg1, long arg2, long arg3, long arg4) {
  register long rax asm("rax") = number;
  register long r10 asm("r10") = arg4;
  asm volatile("syscall"
               : "+a"(rax)
               : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = rax;
  return rax;
}

static inline long __menios_syscall5(long number,
                                     long arg1,
                                     long arg2,
                                     long arg3,
                                     long arg4,
                                     long arg5) {
  register long rax asm("rax") = number;
  register long r10 asm("r10") = arg4;
  register long r8 asm("r8") = arg5;
  asm volatile("syscall"
               : "+a"(rax)
               : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = rax;
  return rax;
}

static inline long __menios_syscall6(long number,
                                     long arg1,
                                     long arg2,
                                     long arg3,
                                     long arg4,
                                     long arg5,
                                     long arg6) {
  register long rax asm("rax") = number;
  register long r10 asm("r10") = arg4;
  register long r8 asm("r8") = arg5;
  register long r9 asm("r9") = arg6;
  asm volatile("syscall"
               : "+a"(rax)
               : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
               : "rcx", "r11", "memory");
  __menios_syscall_last_result = rax;
  return rax;
}

#endif

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H
