#include <kernel/isr.h>
#include <stdio.h>

isr_t interrupt_handlers[256];

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

  printf("intno: %x, errcode: %d, eax: %x, ebx: %x, ecx: %x, edx: %x \n",
    regs.int_no,
    regs.err_code,
    regs.eax,
    regs.ebx,
    regs.ecx,
    regs.edx);

  asm volatile("cli\n"
               "hlt\n");
}

void register_interrupt_handler(uint8_t interrupt, isr_t handler) {
  printf("Adding handler to %x\n", interrupt);
  interrupt_handlers[interrupt] = handler;
}

isr_t get_interrupt_handler(uint8_t interrupt) {
  return interrupt_handlers[interrupt];
}
