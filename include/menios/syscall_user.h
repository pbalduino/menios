#ifndef MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H
#define MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H

#ifdef __cplusplus
extern "C" {
#endif

static inline long __menios_syscall0(long number) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(number)
               : "rcx", "r11", "memory");
  return ret;
}

static inline long __menios_syscall1(long number, long arg1) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(number), "D"(arg1)
               : "rcx", "r11", "memory");
  return ret;
}

static inline long __menios_syscall2(long number, long arg1, long arg2) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(number), "D"(arg1), "S"(arg2)
               : "rcx", "r11", "memory");
  return ret;
}

static inline long __menios_syscall3(long number, long arg1, long arg2, long arg3) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
               : "rcx", "r11", "memory");
  return ret;
}

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_MENIOS_SYSCALL_USER_H
