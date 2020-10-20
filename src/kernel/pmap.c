#include <kernel/console.h>
#include <kernel/isr.h>
#include <kernel/pmap.h>

#include <string.h>
#include <types.h>

#define MEM_ENTRIES 0x400

extern const uint32_t _BSS_START_;

extern void isr14();

page_dir_entry_t page_directory[1024] __attribute__((aligned(4096)));
page_table_entry_t identity_table[1024] __attribute__((aligned(4096)));

void page_fault_handler(registers_t* regs) {
	if(regs){ };
  printf("  Handling page fault: 0x%x\n", regs->int_no);
}

void init_page_directory() {
  uint32_t p = MEM_ENTRIES;

  memset(&page_directory, 0x00, MEM_ENTRIES * sizeof(page_dir_entry_t));
  memset(&identity_table, 0x00, MEM_ENTRIES * sizeof(page_table_entry_t));

  printf("* Initing page directory\n");

  while(p--) {
    page_directory[p].writable = true;
    page_directory[0].zero = 0;
  }

  p = MEM_ENTRIES;
  while(p--) {
    identity_table[p].present = true;
    identity_table[p].writable = true;
    identity_table[p].phys_addr = p;
    identity_table[p].zero = 0;
  }

  page_directory[0].present = true;
  page_directory[0].writable = true;
  page_directory[0].zero = 0;
  page_directory[0].page_table = ((uint32_t)&identity_table) / 0x1000;

  printf("* Enabling paging\n");
  printf("  PD located at 0x%x - 0x%x\n", (uint32_t)&page_directory, page_directory[0].page_table);
  printf("  IT located at 0x%x\n", (uint32_t)&identity_table);

  init_paging((uint32_t)&page_directory);
}

void init_page_fault() {
  printf("* Initing page fault handler\n");
  set_int_handler(ISR_PAGE_FAULT, isr14, GD_KT, IDT_PRESENT | IDT_32BIT_TRAP_GATE);
  printf("OK\n");
}

void init_memory() {
  printf("* Initing memory\n");

  init_page_fault();

  init_page_directory();
}
