#include <kernel/isr.h>
#include <stdio.h>
#include <x86.h>

isr_t interrupt_handlers[256];

const char exceptions[18][32] = {"Divide by zero",
                                  "Debug",
                                  "Non-maskable Interrupt",
                                  "Breakpoint",
                                  "Overflow",
                                  "Bound Range Exceeded",
                                  "Invalid Opcode",
                                  "Device Not Available",
                                  "Double Fault",
                                  "Coprocessor segment overrun",
                                  "Invalid TSS",
                                  "Segment Not Present",
                                  "Stack-Segment Fault",
                                  "General Protection Fault",
                                  "Page Fault",
                                  "<Reserved>",
                                  "x87 Floating-Point Exception",
                                  "Alignment Check"};

void isr_handler(registers_t regs) {
  uint8_t* mem = (uint8_t*)&regs;

  printf("sz: %d\n", sizeof(registers_t));
  for(uint32_t p = 0; p < sizeof(registers_t); p++) {
    if(p && (p % 4 == 0)) {
      printf("\n");
    }
    printf("%x ", mem[p]);
  }
  printf("\n");

  printf("intno: 0x%x (%d - %s), errcode: %d, eax: 0x%x, ebx: 0x%x, ecx: 0x%x, edx: 0x%x, edi: 0x%x, esi: 0x%x, ebp: 0x%x, esp: 0x%x\n",
    regs.int_no,
    regs.int_no,
    exceptions[regs.int_no],
    regs.err_code,
    regs.eax,
    regs.ebx,
    regs.ecx,
    regs.edx,
    regs.edi,
    regs.esi,
    regs.ebp,
    regs.esp);

  outb(0x20, 0x20);
  // asm volatile("cli\n"
  //              "hlt\n");
}

void register_interrupt_handler(uint8_t interrupt, isr_t handler) {
  printf("Adding handler to %x\n", interrupt);
  interrupt_handlers[interrupt] = handler;
}

isr_t get_interrupt_handler(uint8_t interrupt) {
  return interrupt_handlers[interrupt];
}
