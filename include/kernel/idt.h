#ifndef MENIOS_INCLUDE_KERNEL_IDT_H
#define MENIOS_INCLUDE_KERNEL_IDT_H

#include <string.h>
#include <types.h>

#define IDT_KCS 0x08 // 0x08 is the kernel code segment in GDT since each GDT entry is 8 bits

/* Defines an IDT entry */
typedef struct {
    uint16_t base_lo;
    uint16_t sel;        /* Our kernel segment goes here! */
    uint8_t  always0;     /* This will ALWAYS be set to 0! */
    uint8_t  flags;       /* Set using the above table! */
    uint16_t base_hi;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

idt_entry_t idt[256];

idt_ptr_t idtp;

/* This exists in 'start.asm', and is used to load our IDT */
extern void idt_load();

// These extern directives let us access the addresses of our ASM ISR handlers.
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

extern void isr0 ();
extern void isr1 ();
extern void isr2 ();
extern void isr3 ();
extern void isr4 ();
extern void isr5 ();
extern void isr6 ();
extern void isr7 ();
extern void isr8 ();
extern void isr9 ();
extern void isr10 ();
extern void isr11 ();
extern void isr12 ();
extern void isr13 ();
extern void isr14 ();
extern void isr15 ();
extern void isr16 ();
extern void isr17 ();
extern void isr18 ();
extern void isr19 ();

extern void isr20 ();
extern void isr21 ();
extern void isr22 ();
extern void isr23 ();
extern void isr24 ();
extern void isr25 ();
extern void isr26 ();
extern void isr27 ();
extern void isr28 ();
extern void isr29 ();
extern void isr30 ();
extern void isr31 ();

extern void isr128 ();

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void init_idt();

#endif
