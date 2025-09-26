#ifndef MENIOS_INCLUDE_KERNEL_IDT_H
#define MENIOS_INCLUDE_KERNEL_IDT_H

#include <stdbool.h>
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
#define ISR_INVALID_TSS              0x0a
#define ISR_SEGMENT_NOT_PRESENT      0x0b
#define ISR_STACK_SEGMENT_FAULT      0x0c
#define ISR_GENERAL_PROTECTION_FAULT 0x0d
#define ISR_PAGE_FAULT               0x0e
#define ISR_FLOAT_POINT_EXCEPTION    0x10
#define ISR_ALIGNMENT_CHECK          0x11
#define ISR_MACHINE_CHECK            0x12
#define ISR_PERIODIC_TIMER           0x20
#define ISR_KEYBOARD                 0x21
#define ISR_SYSCALL                  0x80

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
typedef struct idt_pointer_t {
  uint16_t  size;         // Size of the IDT - 1
  uintptr_t offset;       // Base address of the IDT
} __attribute__((packed)) idt_pointer_t;

typedef struct idt_exception_t {
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

typedef struct {
  bool present;
  bool write;
  bool user;
  bool reserved;
  bool instruction_fetch;
  bool protection_key;
  bool shadow_stack;
  bool sgx_violation;
} idt_pf_error_info_t;

typedef enum {
  IDT_GPF_TABLE_GDT = 0,
  IDT_GPF_TABLE_IDT = 1,
  IDT_GPF_TABLE_LDT = 2,
  IDT_GPF_TABLE_IDT_2 = 3
} idt_gpf_table_t;

typedef struct {
  bool external;
  bool has_selector;
  idt_gpf_table_t table;
  uint16_t descriptor_index;
} idt_gpf_error_info_t;

extern void idt_df_isr_asm_handler();
extern void idt_generic_isr_asm_handler();
extern void idt_gpf_isr_asm_handler();
extern void idt_load(idt_pointer_t *idt_ptr);
extern void idt_pf_isr_asm_handler();
extern void idt_period_timer_isr_asm_handler();
extern void ps2kb_isr_handler();
extern void syscall_isr_handler();

void idt_add_isr(int interruption, void* handler);
void idt_add_user_isr(int interruption, void* handler);
void idt_init();
static inline void idt_decode_page_fault(uint64_t error_code, idt_pf_error_info_t *info) {
  info->present = (error_code & (1ull << 0)) != 0;
  info->write = (error_code & (1ull << 1)) != 0;
  info->user = (error_code & (1ull << 2)) != 0;
  info->reserved = (error_code & (1ull << 3)) != 0;
  info->instruction_fetch = (error_code & (1ull << 4)) != 0;
  info->protection_key = (error_code & (1ull << 5)) != 0;
  info->shadow_stack = (error_code & (1ull << 6)) != 0;
  info->sgx_violation = (error_code & (1ull << 7)) != 0;
}

static inline void idt_decode_gpf(uint64_t error_code, idt_gpf_error_info_t *info) {
  info->external = (error_code & 0x1ull) != 0;
  info->has_selector = error_code != 0;
  info->table = (idt_gpf_table_t)((error_code >> 1) & 0x3ull);
  info->descriptor_index = (uint16_t)((error_code >> 3) & 0x1fffull);
}

#endif
