#include <kernel/console.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>

idt_pointer_t idt_p;
idt_entry_t idt[0x100];

void set_division_by_zero(){
    uint64_t isr_address = (uint64_t)idt_division_by_zero_isr;

    // Set up the IDT entry
    idt[0].base_low = (uint16_t)(isr_address & 0xFFFF);
    idt[0].selector = KERNEL_CODE_SEGMENT; // Your code segment selector
    idt[0].ist = 0;        // Set to 0 for most cases
    idt[0].type_attr = 0x8e; // 0x8E indicates an interrupt gate (64-bit interrupt gate)
    idt[0].base_mid = (uint16_t)((isr_address >> 16) & 0xFFFF);
    idt[0].base_high = (uint32_t)((isr_address >> 32) & 0xFFFFFFFF);
    idt[0].reserved = 0;
  puts("DIV0 ");
}


void idt_init() {
  puts("- Setting IDT:");
  idt_p.size = sizeof(idt) - 1;
  idt_p.offset = (uint64_t)&idt;
  puts(".");
  set_division_by_zero();

  puts(". ");
  idt_load(&idt_p);

  puts("OK\n");
}

void idt_division_by_zero_handler() {
  puts("\n>>> Exception caught: Division by zero\n");
  // hcf();
}
