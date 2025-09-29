#ifndef MENIOS_INCLUDE_KERNEL_SYSCALL_H
#define MENIOS_INCLUDE_KERNEL_SYSCALL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_LSEEK   8
#define SYS_MMAP    9
#define SYS_MUNMAP 11
#define SYS_PIPE   22
#define SYS_YIELD  24
#define SYS_SLEEP  35
#define SYS_DUP    32
#define SYS_DUP2   33
#define SYS_FORK       57
#define SYS_EXECVE     59
#define SYS_EXIT       60
#define SYS_KILL        62
#define SYS_SIGNAL      63
#define SYS_SIGRETURN   64
#define SYS_SIGACTION   65
#define SYS_SIGPROCMASK 66
#define SYS_FCNTL  72
#define SYS_FB_GETINFO 200
#define SYS_FB_MAP     201
#define SYS_FB_FLIP    202

typedef struct syscall_frame_t {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
} __attribute__((packed)) syscall_frame_t;

typedef uint64_t (*syscall_handler_t)(syscall_frame_t* frame);

void syscall_init(void);
uint64_t syscall_dispatch(syscall_frame_t* frame);

#ifdef __cplusplus
}
#endif

#endif
