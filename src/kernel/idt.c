#include <kernel/console.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>

idt_pointer_t idt_p;
idt_entry_t idt[0x100];

void idt_add_isr(int interruption, void* handler) {
  // Set up the IDT entry
  uintptr_t handler_address = (uintptr_t)handler;

  idt[interruption].base_low = (uint16_t)(handler_address & 0xFFFF);
  idt[interruption].selector = KERNEL_CODE_SEGMENT; // Your code segment selector
  idt[interruption].ist = 0;   // Set to 0 for most cases
  idt[interruption].type_attr = 0x8e; // 0x8E indicates an interrupt gate (64-bit interrupt gate)
  idt[interruption].base_mid = (uint16_t)((handler_address >> 16) & 0xFFFF);
  idt[interruption].base_high = (uint32_t)((handler_address >> 32) & 0xFFFFFFFF);
  idt[interruption].reserved = 0;
  puts(".");
}

void idt_init() {
  puts("- Setting IDT:");
  idt_p.size = sizeof(idt) - 1;
  idt_p.offset = (uint64_t)&idt;
  puts(".");
  idt_add_isr(ISR_DIVISION_BY_ZERO, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_DEBUG, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_BREAKPOINT, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_DOUBLE_FAULT, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_GENERAL_PROTECTION_FAULT, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_PAGE_FAULT, &idt_generic_isr_asm_handler);

  idt_load(&idt_p);

  puts("OK\n");
}

void idt_generic_isr_handler() {
  puts("Exception caught.");
}
