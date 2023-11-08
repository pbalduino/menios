#include <stdint.h>
#include <stdio.h>
#include <boot/limine.h>
#include <kernel/mm.h>
#include <kernel/pmm.h>
#include <kernel/pmap.h>

static pml4_t pml4;
static uintptr_t kernel_offset;

static volatile struct limine_hhdm_request hhdm_request = {
  .id = LIMINE_HHDM_REQUEST,
  .revision = 0
};

static inline uintptr_t physical_to_virtual(uintptr_t physical_address) {
  return physical_address + kernel_offset;
}

static inline uintptr_t virtual_to_physical(uintptr_t virtual_to_physical) {
  return virtual_to_physical - kernel_offset;
}


uint64_t read_cr3() {
  uint64_t value;
  asm volatile("movq %%cr3, %0" : "=r" (value));
  return value;
}

void write_cr3(uint64_t value) {
  asm volatile("movq %0, %%cr3" : : "r" (value));
}

void create_empty_pml4() {
  for(int e = 0; e < 512; e++) {
    pml4.entries[e].present = 0;
  }
}

void pml4_entry(page_directory_pointer_t* pdp, uint32_t pml4_index, uint32_t flags) {
  if(pml4.entries[pml4_index].present) {
    pdp = (page_directory_pointer_t*)physical_to_virtual(pml4.entries[pml4_index].page_directory_base);
    printf("  pdp found: %lx", &pdp);
  } else {
    printf("  pdp not found.\n");
  }
}

void map_kernel_page(uintptr_t virtual_address, uint32_t flags) {
  int pml4_index = (virtual_address >> 39) & 0x1FF;
  int pdp_index = (virtual_address >> 30) & 0x1FF;
  int pd_index = (virtual_address >> 21) & 0x1FF;
  int pt_index = (virtual_address >> 12) & 0x1FF;
  uint64_t pt_frame = virtual_to_physical(virtual_address) >> 12;
  page_directory_pointer_t* pdp;
  page_directory_t* pd;
  page_table_t* pt;

  // directory_pointer(pdp, pml4_index, flags);
}

void pml4_init() {
  create_empty_pml4();

  map_kernel_page(&pml4, PD_PRESENT | PD_WRITABLE);

  uint64_t phys = virtual_to_physical((uint64_t) &pml4);
  printf("  %lx phys: - sz: %d\n", phys, sizeof(pml4));

  write_cr3(phys);
  printf("  48 ");
}

void mm_init() {
  kernel_offset = hhdm_request.response->offset;

  printf("- Starting memory management. HHDM: %lx\n", kernel_offset);

  pmap_init();

  pml4_init();
}
