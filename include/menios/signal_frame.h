#ifndef MENIOS_INCLUDE_SIGNAL_FRAME_H
#define MENIOS_INCLUDE_SIGNAL_FRAME_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menios_signal_context {
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
} __attribute__((packed)) menios_signal_context_t;

typedef struct menios_signal_frame {
  menios_signal_context_t context;
  uint64_t signal_mask;
  int32_t  signo;
  int32_t  reserved;
} __attribute__((packed)) menios_signal_frame_t;

#ifdef __cplusplus
}
#endif

#endif
