#include <kernel/idt.h>
#include <kernel/pic.h>
#include <stdio.h>

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
  /* The interrupt routine's base address */
  idt[num].base_lo = (base & 0xFFFF);
  idt[num].base_hi = (base >> 16) & 0xFFFF;

  /* The segment or 'selector' that this IDT entry will use
  *  is set here, along with any access flags */
  idt[num].sel = sel;
  idt[num].always0 = 0;
  idt[num].flags = flags;
}

/* Installs the IDT */
void init_idt() {
  /* Sets the special IDT pointer up */
  idtp.limit = (sizeof (struct idt_entry) * 256) - 1;
  idtp.base = (uint32_t)&idt;

  /* Clear out the entire IDT, initializing it to zeros */
  memset(&idt, 0, sizeof(struct idt_entry) * 256);

  /* Add any new ISRs to the IDT here using idt_set_gate */
  idt_set_gate(0x00, (uint32_t) isr0, IDT_KCS, 0x8E);
  // idt_set_gate(0x01, (uint32_t) isr1, IDT_KCS, 0x8E);
  // idt_set_gate(0x02, (uint32_t) isr2, IDT_KCS, 0x0E); // reserved, present
  // idt_set_gate(0x03, (uint32_t) isr3, IDT_KCS, 0x8E);
  // idt_set_gate(0x04, (uint32_t) isr4, IDT_KCS, 0x8E);
  // idt_set_gate(0x05, (uint32_t) isr5, IDT_KCS, 0x8E);
  idt_set_gate(0x06, (uint32_t) isr6, IDT_KCS, 0x8E);
  // idt_set_gate(0x07, (uint32_t) isr7, IDT_KCS, 0x8E);
  idt_set_gate(0x08, (uint32_t) isr8, IDT_KCS, 0x8E);
  // idt_set_gate(0x09, (uint32_t) isr9, IDT_KCS, 0x8E);
  // idt_set_gate(0x0a, (uint32_t) isr10, IDT_KCS, 0x8E);
  // idt_set_gate(0x0b, (uint32_t) isr11, IDT_KCS, 0x8E);
  // idt_set_gate(0x0c, (uint32_t) isr12, IDT_KCS, 0x8E);
  idt_set_gate(0x0d, (uint32_t) isr13, IDT_KCS, 0x8E);
  // idt_set_gate(0x0e, (uint32_t) isr14, IDT_KCS, 0x8E);
  // idt_set_gate(0x0f, (uint32_t) isr15, IDT_KCS, 0x0E); // reserved, present
  // idt_set_gate(0x10, (uint32_t) isr16, IDT_KCS, 0x8E);
  // idt_set_gate(17, (uint32_t) isr17, IDT_KCS, 0x8E);
  // idt_set_gate(18, (uint32_t) isr18, IDT_KCS, 0x8E);
  // idt_set_gate(19, (uint32_t) isr19, IDT_KCS, 0x8E);
  // idt_set_gate(20, (uint32_t) isr20, IDT_KCS, 0x8E);
  // idt_set_gate(21, (uint32_t) isr21, IDT_KCS, 0x8E);
  // idt_set_gate(22, (uint32_t) isr22, IDT_KCS, 0x8E);
  // idt_set_gate(23, (uint32_t) isr23, IDT_KCS, 0x8E);
  // idt_set_gate(24, (uint32_t) isr24, IDT_KCS, 0x8E);
  // idt_set_gate(25, (uint32_t) isr25, IDT_KCS, 0x8E);
  // idt_set_gate(26, (uint32_t) isr26, IDT_KCS, 0x8E);
  // idt_set_gate(27, (uint32_t) isr27, IDT_KCS, 0x8E);
  // idt_set_gate(28, (uint32_t) isr28, IDT_KCS, 0x8E);
  // idt_set_gate(29, (uint32_t) isr29, IDT_KCS, 0x8E);
  // idt_set_gate(30, (uint32_t) isr30, IDT_KCS, 0x8E);
  // idt_set_gate(31, (uint32_t) isr31, IDT_KCS, 0x8E);
  idt_set_gate(0x80, (uint32_t) isr128, IDT_KCS, 0x8E);

  idt_set_gate(0x20, (uint32_t) irq0, IDT_KCS, 0x8E);
  idt_set_gate(0x21, (uint32_t) irq1, IDT_KCS, 0x8E);
  // idt_set_gate(34, (uint32_t) irq2, IDT_KCS, 0x8E);
  // idt_set_gate(35, (uint32_t) irq3, IDT_KCS, 0x8E);
  // idt_set_gate(36, (uint32_t) irq4, IDT_KCS, 0x8E);
  // idt_set_gate(37, (uint32_t) irq5, IDT_KCS, 0x8E);
  // idt_set_gate(38, (uint32_t) irq6, IDT_KCS, 0x8E);
  // idt_set_gate(39, (uint32_t) irq7, IDT_KCS, 0x8E);
  // idt_set_gate(40, (uint32_t) irq8, IDT_KCS, 0x8E);
  // idt_set_gate(41, (uint32_t) irq9, IDT_KCS, 0x8E);
  // idt_set_gate(42, (uint32_t) irq10, IDT_KCS, 0x8E);
  // idt_set_gate(43, (uint32_t) irq11, IDT_KCS, 0x8E);
  // idt_set_gate(44, (uint32_t) irq12, IDT_KCS, 0x8E);
  // idt_set_gate(45, (uint32_t) irq13, IDT_KCS, 0x8E);
  // idt_set_gate(46, (uint32_t) irq14, IDT_KCS, 0x8E);
  // idt_set_gate(47, (uint32_t) irq15, IDT_KCS, 0x8E);

  /* Points the processor's internal register to the new IDT */
  printf("Defining interruption table\n");
  idt_load();
}
