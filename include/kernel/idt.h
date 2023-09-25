#ifndef MENIOS_INCLUDE_KERNEL_IDT_H
#define MENIOS_INCLUDE_KERNEL_IDT_H

#include <types.h>

typedef struct {
  uint16_t base_low;      // Lower 16 bits of ISR address
  uint16_t selector;      // Code segment selector
  uint8_t  ist;           // Interrupt Stack Table index (set to 0 for most cases)
  uint8_t  type_attr;     // Type and attributes (e.g., interrupt gate)
  uint16_t base_mid;      // Middle 16 bits of ISR address
  uint32_t base_high;     // Upper 32 bits of ISR address
  uint32_t reserved;      // Reserved (set to 0)
} idt_entry_t;

// Define the IDT pointer structure
typedef struct  __attribute__((packed)) {
  uint16_t size;         // Size of the IDT - 1
  uint64_t offset;       // Base address of the IDT
} idt_pointer_t;

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t isr_number;
    uint64_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} idt_exception_t;

extern void idt_division_by_zero_isr();
extern void idt_load(idt_pointer_t *idt_ptr);

void idt_division_by_zero_handler();
void idt_init();

#endif
