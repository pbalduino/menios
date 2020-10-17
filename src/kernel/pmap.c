#include <kernel/console.h>
#include <kernel/isr.h>
#include <kernel/pmap.h>

#include <string.h>
#include <types.h>

#define MEM_ENTRIES 0x400

extern const uint32_t _BSS_START_;

extern void isr_page_fault();

uint32_t* page_directory = (uint32_t*)0x1f000;
// page_table_entry_t pt[MEM_ENTRIES];

void page_fault_handler(registers_t* regs) {
	if(regs){ };
	// clean_buffer();
	// irq_eoi();
  printf("  Handling page fault\n");
}

void init_page_directory() {
  uint32_t p = MEM_ENTRIES;

  memset(page_directory, 0x00, MEM_ENTRIES * sizeof(uint32_t));

  printf("* Initing page directory\n");

  while(p--) {
    *(page_directory + p) = PD_WRITABLE;
  }

  p = MEM_ENTRIES;
  while(p--) {
    if(*(page_directory + p) != PD_WRITABLE) {
      printf("  Incorrect entry @ %d\n", p);
    }
  }

  printf("* Enabling paging\n");
  printf("  PD located at 0x%x\n", (uint32_t)page_directory);

  init_paging(page_directory);
}

void init_page_fault() {
  printf("* Initing page fault handler\n");
  set_int_handler(ISR_PAGE_FAULT, isr_page_fault, GD_KT, IDT_PRESENT | IDT_32BIT_INTERRUPT_GATE);
}

void init_memory() {
  printf("* Initing memory\n");

  init_page_fault();

  init_page_directory();
/*
  p = MEM_ENTRIES;

  while(p--) {
    pt[p].phys_addr = (p * 0x1000);
    pt[p].writable = true;
    pt[p].present = true;
  }

  page_directory[0].present = true;
  page_directory[0].page_table = (uintptr_t)&pt;
*/
}
