#ifndef MENIOS_INCLUDE_KERNEL_IRQ_H
#define MENIOS_INCLUDE_KERNEL_IRQ_H

#include <kernel/interrupts.h>

#define IDT_32BIT_INTERRUPT_GATE 0x0e
#define IDT_STORAGE_SEGMENT      0x20
#define IDT_DPL_3                0x60
#define IDT_PRESENT              0x80

#define PIC1_COMM                0x20
#define PIC1_DATA                PIC1_COMM + 1

#define PIC2_COMM                0xa0
#define PIC2_DATA                PIC2_COMM + 1

#define ICW1                     0x11
#define ICW2_MASTER              0x20 // put IRQs 0-7 at 0x20 (above Intel reserved ints)
#define ICW2_SLAVE               0x28 // put IRQs 8-15 at 0x28
#define ICW3_MASTER              0x04 // IR2 connected to slave
#define ICW3_SLAVE               0x02 // slave has id 2
#define ICW4                     0x01 // 8086 mode, no auto-EOI, non-buffered mode,

#define IRQ_OFFSET               0x20
#define IRQ_TIMER                0x00
#define IRQ_KEYBOARD             0x01

void init_irq();

void init_idt();

void remap_irq();

void irq_set_mask(uint8_t irq_line);

void set_irq_handler(uint8_t irq_line, isr_t handler, uint16_t segment, uint8_t flags);

void irq_eoi();

#endif
