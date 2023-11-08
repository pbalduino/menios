#include <boot/limine.h>
#include <kernel/kernel.h>
#include <kernel/pmap.h>
#include <kernel/serial.h>

#include <stdio.h>

static volatile struct limine_memmap_request memmap_request = {
  .id = LIMINE_MEMMAP_REQUEST,
  .revision = 0
};

static struct limine_memmap_response* memmap;

static uint64_t mem_total = 0;
static uint64_t mem_available = 0;
static uint64_t mem_framebuffer = 0;
static uint64_t kernel_offset;

static pml4_t pmapl4;
static page_directory_pointer_t pdpb;
static page_directory_t pdb;
static page_table_t ptb;

static char* mem_type[8] = {
  "Usable",
  "Reserved",
  "ACPI reclaimable",
  "ACPI NVS",
  "Bad memory",
  "Bootloader reclaimable",
  "Kernel and modules",
  "Framebuffer"
};
/*
uint64_t read_cr3() {
  uint64_t value;
  asm volatile("movq %%cr3, %0" : "=r" (value));
  return value;
}

void write_cr3(uint64_t value) {
  asm volatile("movq %0, %%cr3" : : "r" (value));
}

static inline uintptr_t physical_to_virtual(uintptr_t physical_address) {
  return physical_address + kernel_offset;
}

static inline uintptr_t virtual_to_physical(uintptr_t virtual_to_physical) {
  return virtual_to_physical - kernel_offset;
}
*/
void pmap_init() {

  printf("- Starting memory management");
  if(memmap_request.response == NULL || memmap_request.response->entry_count < 1) {
    printf(" > Error mapping the memory. Halting.");
    hcf();
  }

  memmap = memmap_request.response;

  // if(hhdm_request.response == NULL) {

  // }

  // kernel_offset = hhdm_request.response->offset;

  printf(". %lu entries found..\n", memmap->entry_count);

  for(uint64_t e = 0; e < memmap->entry_count; e++) {
    printf("  Entry %lu:  base: %lx - size: %lu (%lx) - type: %s\n", e, memmap->entries[e]->base, memmap->entries[e]->length, memmap->entries[e]->length, mem_type[memmap->entries[e]->type]);
    switch (memmap->entries[e]->type) {
    case LIMINE_MEMMAP_USABLE:
      mem_available += memmap->entries[e]->length;
      break;
    case LIMINE_MEMMAP_FRAMEBUFFER:
    mem_framebuffer += memmap->entries[e]->length;
      break;
    default:
      break;
    }

    mem_total += memmap->entries[e]->length;
  }

  // printf(". Kernel starts @ %lx\n", kernel_offset);

/*
  serial_printf("  Total: %luMB - available: %luMB - video: %luMB\n", mem_total / (1024 * 1024), mem_available / (1024 * 1024), mem_framebuffer / (1024 * 1024));

  serial_puts("It's gonna blow\n");

  uintptr_t virtual_address = &pmapl4;

  printf("New CR3: %lx\n", virtual_address);

  int pml4_index = (virtual_address >> 39) & 0x1FF;
  int pdp_index = (virtual_address >> 30) & 0x1FF;
  int pd_index = (virtual_address >> 21) & 0x1FF;
  int pt_index = (virtual_address >> 12) & 0x1FF;
  uint64_t pt_frame = virtual_to_physical(virtual_address) >> 12;

  printf("97 ");

  ptb.entries[pt_index].present = 1;
  ptb.entries[pt_index].writable = 1;
  ptb.entries[pt_index].no_execute = 1;
  ptb.entries[pt_index].user = 0;
  ptb.entries[pt_index].frame = pt_frame;

  printf("105 ");

  pdb.entries[pd_index].present = 1;
  pdb.entries[pd_index].page_table_base = virtual_to_physical(&ptb);

  printf("110 ");

  pdpb.entries[pdp_index].page_directory_base = virtual_to_physical(&pdb);
  pdpb.entries[pdp_index].present = 1;

  printf("115 ");

  pmapl4.entries[pml4_index].present = 1;
  pmapl4.entries[pml4_index].page_directory_base = virtual_to_physical(&pdpb);

  printf("120: %lx - %lx ", virtual_address, virtual_to_physical(virtual_address));

  write_cr3(virtual_to_physical(virtual_address));

  printf(" 124\n");

  printf("pml4: %d - pdp: %d - pd: %d - pt: %d\n", pml4_index, pdp_index, pd_index, pt_index);

  printf("paddr: %lx\n", physical_to_virtual(virtual_address));

  serial_puts("Did it work?\n");
  */
}
