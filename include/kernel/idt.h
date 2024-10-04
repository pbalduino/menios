#ifndef MENIOS_INCLUDE_KERNEL_IDT_H
#define MENIOS_INCLUDE_KERNEL_IDT_H

#include <types.h>

#define ISR_DIVISION_BY_ZERO         0x00
#define ISR_DEBUG                    0x01
#define ISR_NON_MASKABLE             0x02
#define ISR_BREAKPOINT               0x03
#define ISR_OVERFLOW                 0x04
#define ISR_BOUND_RANGE_EXCEEDED     0x05
#define ISR_INVALID_OPCODE           0x06
#define ISR_DEVICE_NOT_AVAILABLE     0x07
#define ISR_DOUBLE_FAULT             0x08
#define ISR_INVALID_TSS              0x0A
#define ISR_SEGMENT_NOT_PRESENT      0x0B
#define ISR_STACK_SEGMENT_FAULT      0x0C
#define ISR_GENERAL_PROTECTION_FAULT 0x0D
#define ISR_PAGE_FAULT               0x0E
#define ISR_FLOAT_POINT_EXCEPTION    0x10
#define ISR_ALIGNMENT_CHECK          0x11
#define ISR_MACHINE_CHECK            0x12
#define ISR_PERIODIC_TIMER           0x20

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

    uint64_t rflags;

    uint64_t error_code;
} idt_exception_t;

typedef idt_exception_t* idt_exception_p;

extern void idt_df_isr_asm_handler();
extern void idt_generic_isr_asm_handler();
extern void idt_gpf_isr_asm_handler();
extern void idt_load(idt_pointer_t *idt_ptr);
extern void idt_pf_isr_asm_handler();
extern void idt_period_timer_isr_asm_handler();

void idt_add_isr(int interruption, void* handler);
void idt_init();

#endif
