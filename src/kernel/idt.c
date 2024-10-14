#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>
#include <kernel/heap.h>
#include <kernel/serial.h>

#include <stdio.h>

idt_pointer_t idt_p;
idt_entry_t idt[0x100];

void idt_add_isr(int interruption, void* handler) {
  // Set up the IDT entry
  uintptr_t handler_address = (uintptr_t)handler;

  idt[interruption].base_low = (uint16_t)(handler_address & 0xFFFF);
  idt[interruption].selector = KERNEL_CODE_SEGMENT; // Your code segment selector
  idt[interruption].ist = 0;   // Set to 0 for most cases
  idt[interruption].type_attr = 0x8e; // 0x8E indicates an interrupt gate (64-bit interrupt gate)
  idt[interruption].base_mid = (uint16_t)((handler_address >> 16) & 0xffff);
  idt[interruption].base_high = (uint32_t)((handler_address >> 32) & 0xffffffff);
  idt[interruption].reserved = 0;
  puts(".");
}

void idt_init() {
  puts("- Setting IDT");
  idt_p.size = sizeof(idt) - 1;
  idt_p.offset = (uint64_t)&idt;
  idt_add_isr(ISR_DIVISION_BY_ZERO, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_DEBUG, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_BREAKPOINT, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_DOUBLE_FAULT, &idt_df_isr_asm_handler);
  idt_add_isr(ISR_GENERAL_PROTECTION_FAULT, &idt_gpf_isr_asm_handler);
  idt_add_isr(ISR_PAGE_FAULT, &idt_pf_isr_asm_handler);
  idt_add_isr(ISR_PERIODIC_TIMER, &idt_period_timer_isr_asm_handler);

  idt_load(&idt_p);

  puts("OK\n");
}

void idt_generic_isr_handler() {
  puts("= Exception caught.");
  serial_printf("== Exception caught ==\n");
  hcf();
}

void idt_df_isr_handler() {
  puts("= Double fault caught.\n");
  serial_printf("== Double fault raised ==\n");
  hcf();
}

void idt_gpf_isr_handler(idt_exception_p cpu_state) {
  puts("= General protection fault caught.\n");
  serial_printf("== General protection fault: %p ==\n", cpu_state);
  serial_printf("  R15: %lx error: %lx\n", cpu_state->r15, cpu_state->error_code);
  dump_heap((heap_node_p)(void*)cpu_state, sizeof(idt_exception_t));
  hcf();
}

void idt_pf_isr_handler(uint64_t error_code) {
  puts("= Page fault caught.\n");
  uint64_t faulting_address;
  int present    = (error_code & 0x01);
  int write      = (error_code & 0x02) >> 1;
  int user_mode  = (error_code & 0x04) >> 2;
  int reserved   = (error_code & 0x08) >> 3;
  // int fetch      = (error_code & 0x10) >> 4;
  // int protection = (error_code & 0x20) >> 5;
  // int shadow     = (error_code & 0x40) >> 6;
  int index      = (error_code & 0xff0) >> 4;

  asm volatile ("movq %%cr2, %0" : "=r" (faulting_address));

  serial_puts("Page fault caught trying to access address:\n");

  printf("- Page fault caught trying to access address %lx. Error code: %ld\n", faulting_address, error_code);
  printf("  Index %d\n", index);
  printf("  Present: %d, Write: %d, User Mode: %d, Reserved: %d\n", present, write, user_mode, reserved);
  serial_printf("  Present: %d, Write: %d, User Mode: %d, Reserved: %d\n", present, write, user_mode, reserved);

  hcf();
}
