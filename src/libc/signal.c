#ifndef MENIOS_KERNEL
#include <uapi/signal.h>
#include <kernel/syscall.h>
#include <sys/errno.h>
#include <unistd.h>

#define __MENIOS_STR(x) #x
#define __MENIOS_XSTR(x) __MENIOS_STR(x)

typedef void (*handler_t)(int);

__attribute__((noreturn, naked)) void __menios_sigreturn_trampoline(void) {
  __asm__ volatile(
    "mov %rsp, %rdi\n\t"
    "mov $" __MENIOS_XSTR(SYS_SIGRETURN) ", %rax\n\t"
    "int $0x80\n\t"
    "mov $" __MENIOS_XSTR(SYS_EXIT) ", %rax\n\t"
    "xor %rdi, %rdi\n\t"
    "xor %rsi, %rsi\n\t"
    "xor %rdx, %rdx\n\t"
    "int $0x80\n\t"
    "ud2\n\t"
  );
}

handler_t signal(int sig, handler_t handler) {
  register uint64_t rax asm("rax") = SYS_SIGNAL;
  register uint64_t rdi asm("rdi") = (uint64_t)sig;
  register uint64_t rsi asm("rsi") = (uint64_t)handler;
  register uint64_t rdx asm("rdx") = (uint64_t)&__menios_sigreturn_trampoline;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return SIG_ERR;
  }

  errno = 0;
  return (handler_t)rax;
}

int sigreturn(void* frame) {
  register uint64_t rax asm("rax") = SYS_SIGRETURN;
  register void* rdi asm("rdi") = frame;

  asm volatile("int $0x80"
               : "+a"(rax)
               : "D"(rdi)
               : "rcx", "r11", "memory");

  if((int64_t)rax < 0) {
    errno = (int)(-((int64_t)rax));
    return -1;
  }

  errno = 0;
  return 0;
}

#endif
