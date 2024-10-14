#include <boot/limine.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <kernel/mman.h>

static uint64_t page_bitmap[PAGE_BITMAP_SIZE];
static uintptr_t kernel_offset;
static uintptr_t cr3_vaddr;

static volatile struct limine_hhdm_request hhdm_request = {
  .id = LIMINE_HHDM_REQUEST,
  .revision = 0
};

static volatile struct limine_memmap_request memmap_request = {
  .id = LIMINE_MEMMAP_REQUEST,
  .revision = 0
};

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

uintptr_t physical_to_virtual(uintptr_t physical_address) {
  return physical_address + kernel_offset;
}

uintptr_t virtual_to_physical(uintptr_t virtual_address) {
  return virtual_address - kernel_offset;
}

uintptr_t read_cr2() {
  uintptr_t value;
  asm volatile("movq %%cr2, %0" : "=r" (value));
  return value;
}

void set_page_free(uintptr_t physical_address) {
  size_t page_number = physical_address / PAGE_SIZE;
  size_t index = page_number / (sizeof(uint64_t) * 8);
  size_t bit_position = page_number % (sizeof(uint64_t) * 8);
  page_bitmap[index] &= ~(1UL << bit_position);
}

void set_page_used(uintptr_t physical_address) {
  size_t page_number = physical_address / PAGE_SIZE;
  size_t index = page_number / (sizeof(uint64_t) * 8);
  size_t bit_position = page_number % (sizeof(uint64_t) * 8);
  page_bitmap[index] |= (1UL << bit_position);
}

void set_page_row_free(uintptr_t physical_address) {
  size_t page_number = physical_address / PAGE_SIZE;
  size_t index = page_number / (sizeof(unsigned long) * 8);
  page_bitmap[index] = PAGE_FREE;
}

uint8_t get_page_status(uintptr_t physical_address) {
  serial_printf("get_page_status: %d\n", __LINE__);
  size_t page_number = physical_address / PAGE_SIZE;
  size_t index = page_number / (sizeof(uint64_t) * 8);
  size_t bit_position = page_number % (sizeof(uint64_t) * 8);
  
  serial_printf("get_page_status: %d - idx %d - pa %lx\n", __LINE__, index, physical_address);
  return (page_bitmap[index] & (1UL << bit_position)) ? PAGE_USED : PAGE_FREE;
}

void bulk_page_bitmap_as_free(uint64_t base, uint64_t size) {
  if(size == 0) {
    return;
  }

  if(base % PAGE_SIZE == 0 && size % PAGE_SIZE == 0) {
    for(uint64_t p = base; p < base + size; p += PAGE_SIZE) {
      uint64_t b = p - base;
      uint64_t r = size - b;

      if(p % PAGE_ROW_SIZE == 0 && r > PAGE_ROW_SIZE) {
        set_page_row_free(p);
        p += (PAGE_ROW_SIZE - PAGE_SIZE);
      }

      set_page_free(p);
    }
  } else {
    serial_printf("bulk_page_bitmap_as_free what?\n");
  }
}

void list_memory_areas() {
  uint64_t mem_total;
  uint64_t mem_available;
  uint64_t mem_framebuffer;

  struct limine_memmap_response* memmap_response;

  if(memmap_request.response == NULL) {
    printf("== error reading limine_memmap_response ==\n");
    serial_error("== error reading limine_memmap_response ==\n");
    hcf();
  }

   memmap_response = memmap_request.response;
  
  for(uint64_t e = 0; e < memmap_response->entry_count; e++) {
    serial_printf("  Entry %lu:  base: %lx - size: %lu (%lx) - type: %s\n", e, memmap_response->entries[e]->base, 
      memmap_response->entries[e]->length, 
      memmap_response->entries[e]->length, 
      mem_type[memmap_response->entries[e]->type]);

    switch (memmap_response->entries[e]->type) {
    case LIMINE_MEMMAP_USABLE:
      mem_available += memmap_response->entries[e]->length;
      bulk_page_bitmap_as_free(memmap_response->entries[e]->base, memmap_response->entries[e]->length);
      break;
    case LIMINE_MEMMAP_FRAMEBUFFER:
      mem_framebuffer += memmap_response->entries[e]->length;
      break;
    default:
      break;
    }

    mem_total += memmap_response->entries[e]->length;
  }

  serial_printf("  Total: %luMB - available: %luMB - video: %luMB\n", 
    mem_total / (1024 * 1024), 
    mem_available / (1024 * 1024), 
    mem_framebuffer / (1024 * 1024));
}

void init_kernel_offset() {
  serial_printf("> init_kernel_offset\n");
  kernel_offset = hhdm_request.response->offset;
  serial_printf("  Kernel offset %lx:\n", kernel_offset);
}

virt_addr_t get_kernel_offset() {
  return kernel_offset;
}

void init_page_bitmap() {
  memsetl(&page_bitmap, PAGE_BITMAP_FULL, PAGE_BITMAP_SIZE);
}

uint64_t get_first_free_page() {
  serial_printf("get_first_free_page\n");

  for(size_t p = 0; p < PAGE_BITMAP_SIZE; p++) {
    uint64_t page_row = page_bitmap[p];
    if(page_row != PAGE_BITMAP_FULL) {
      for(int b = 0; b < 64; b++) {
        if((page_row & (1UL << b)) == 0) {
          serial_printf("get_first_free_page: first free page: %d - %lx - %lx - %lx\n", b, 
            p * PAGE_ROW_SIZE, 
            page_row, 
            (p * PAGE_ROW_SIZE) + (b * PAGE_SIZE));

          return (p * PAGE_ROW_SIZE) + (b * PAGE_SIZE);
        }
      }
    }
  }

  serial_printf("get_first_free_page: no free page found\n");

  return 0L;
}

phys_addr_t read_cr3() {
  phys_addr_t value;
  asm volatile("movq %%cr3, %0" : "=r" (value));
  return value;
}

void init_cr3() {
  cr3_vaddr = physical_to_virtual(read_cr3());
}

void pml4_map(uintptr_t vaddr, pml4_map_t* map) {
  pml4_t* root = (pml4_t*)cr3_vaddr;
  map->pml4 = (vaddr >> 39) & 0x1ff;
  map->pdpt = (vaddr >> 30) & 0x1ff;
  map->pd = (vaddr >> 21) & 0x1ff;
  map->pt = (vaddr >> 12) & 0x1ff;
  map->offset = vaddr & 0xfff;
}

uintptr_t get_first_free_virtual_address(uintptr_t offset) {
  serial_printf("get_first_free_virtual_address: offset: %lx\n", offset);
  pml4_t* root = (pml4_t*)cr3_vaddr;

  pml4_map_t map;

  pml4_map(offset, &map);

  serial_printf("get_first_free_virtual_address: pml4e %lx - pdpte %lx - pde %lx - pte %lx - offset %lx\n",
    map.pml4,
    map.pdpt,
    map.pd,
    map.pt,
    map.offset);

  for(uint64_t pml4_e = (offset >> 39) & 0x1ff; pml4_e < 0x200; pml4_e++) {
    if(!root->entries[pml4_e].present) {
      uintptr_t base = pml4_e << 39;
      serial_printf("found: pml4e %lx\n", base);
      return base | VADDR_UNUSED;
    }

    page_directory_pointer_t* pdpt = 
      (page_directory_pointer_t*)physical_to_virtual(root->entries[pml4_e].page_directory_base << 12);

    for(uint64_t pdpt_e = (offset >> 30) & 0x1ff; pdpt_e < 0x200; pdpt_e++) {
      if(!pdpt->entries[pdpt_e].present) {
        uintptr_t base = (pml4_e << 39) | (pdpt_e << 30);
        serial_printf("get_first_free_virtual_address: found: pdpte: %lx(%lx) | %lx(%lx) - %lx\n", pml4_e, pml4_e << 39, pdpt_e, pdpt_e << 30, base);
        return base | VADDR_UNUSED;
      }

      page_directory_t* pd = 
        (page_directory_t*)physical_to_virtual(pdpt->entries[pdpt_e].page_directory_base << 12);

      for(uint64_t pd_e = (offset >> 21) & 0x1ff; pd_e < 0x200; pd_e++) {
        if(!pd->entries[pd_e].present) {
          uintptr_t base = (pml4_e << 39) | (pdpt_e << 30) | (pd_e << 21);
          serial_printf("get_first_free_virtual_address: found: pde: %lx(%lx) | %lx(%lx) | %lx(%lx) - %lx\n", 
            pml4_e, 
            pml4_e << 39, 
            pdpt_e, 
            pdpt_e << 30, 
            pd_e, 
            pd_e << 21, 
            base);

          return base | VADDR_UNUSED;
        }

        page_table_t* pt = (page_table_t*)physical_to_virtual(pd->entries[pd_e].page_table_base << 12);

        for(uint64_t pt_e = (offset >> 12) & 0x1ff; pt_e < 0x200; pt_e++) {
          if(!pt->entries[pt_e].present) {
            uintptr_t base = (pml4_e << 39) | (pdpt_e << 30) | (pd_e << 21) | (pt_e << 12);
            serial_printf("      found: pte: %lx(%lx) | %lx(%lx) | %lx(%lx) | %lx(%lx) - %lx\n", 
              pml4_e, 
              pml4_e << 39, 
              pdpt_e, 
              pdpt_e << 30, 
              pd_e, 
              pd_e << 21, 
              pt_e, 
              pt_e << 12, 
              base);

            return base | VADDR_UNUSED;
          }
        }
      }
    }
  }
  serial_printf("get_first_free_virtual_address: WWWWWWHHHHHHHYYYYYYY?");
  return 0L;
}

/*
- define the handler for page fault
- set the bitmap with all pages as PAGE_USED
- map the available memory using data from Limine
- set the free memory addresses as free pages in the bitmap
- load the used pages and set as PAGE_USED in bitmap
- test a page fault:
  - get the equivalent page in bitmap and mark as PAGE_USED
  - set the equivalent page in CR3 as writable
  - set any value in the request address
  - read the value in the request address
*/
void pmm_init() {
  printf("- Initing Physical memory manager");
  serial_puts("\n- Initing Physical memory manager:");

  init_page_bitmap();
  puts(".");

  list_memory_areas();
  puts(".");
  
  init_kernel_offset();
  puts(".");

  init_cr3();
  puts(".");

  printf("OK\n");
}


// Page Fault handler stub
void page_fault_handler(uint64_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags, uint64_t rsp, uint64_t ss) {
    uint64_t cr2 = read_cr2();

    // Log the fault information
    serial_printf("Page Fault occurred!\n");
    serial_printf("  Error code: %lx\n", error_code);
    serial_printf("  Address:    %lx\n", cr2);
    
    printf("Page Fault occurred!\n");
    printf("  Error code: %lx\n", error_code);
    printf("  Instruction pointer (RIP): %lx\n", rip);
    printf("  Code segment (CS): %lx\n", cs);
    printf("  RFLAGS: %lx\n", rflags);
    printf("  Stack pointer (RSP): %lx\n", rsp);
    printf("  Stack segment (SS): %lx\n", ss);
    printf("  Faulty address (CR2): %lx", cr2);

    // Handle the page fault (This is where you would add your logic)
    // For now, we'll just halt the CPU
    while(1) {
        __asm__("hlt");
    }    
}