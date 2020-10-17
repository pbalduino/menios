#ifndef MENIOS_INCLUDE_KERNEL_INTERRUPTS_H
#define MENIOS_INCLUDE_KERNEL_INTERRUPTS_H

#include <types.h>

#define GD_KT     0x08 // Kernel text

#define IDT_32BIT_INTERRUPT_GATE 0x0e
#define IDT_STORAGE_SEGMENT      0x20
#define IDT_DPL_3                0x60
#define IDT_PRESENT              0x80

typedef struct {
  uint16_t base_lo;
  uint16_t segment;
  uint8_t  zero;
  uint8_t  type_attr;
  uint16_t base_hi;
} __attribute__((packed)) idt_entry_t;

typedef struct {
  uint16_t  limit;      /* Size of IDT array - 1 */
  uintptr_t base;       /* Pointer to IDT array  */
} __attribute__((packed)) idtp_t;

typedef struct {
   uint32_t ds;                                     // Data segment selector
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
   uint32_t int_no, err_code;                       // Interrupt number and error code (if applicable)
   uint32_t eip, cs, eflags, useresp, ss;           // Pushed by the processor automatically.
} registers_t;

typedef void (*isr_t)(registers_t*);

void set_int_handler(uint8_t int_no, isr_t handler, uint16_t segment, uint8_t flags);

#endif
