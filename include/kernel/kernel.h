#ifndef _KERNEL_H
#define _KERNEL_H 1

#include <stdint.h>

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, cs, rflags, rsp, ss;
    uint64_t error_code;
} stack_frame_t;

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

void disable_interrupts();
void enable_interrupts();
void halt() __attribute__((noreturn));
void noop();
void outb(uint16_t port, uint8_t value);
void outl(uint16_t port, uint32_t value);
void outw(uint16_t port, uint16_t value);

#endif
