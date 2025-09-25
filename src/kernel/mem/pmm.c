#include <boot/limine.h>

#include <kernel/console.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

virt_addr_t physical_to_virtual(phys_addr_t physical_address) {
  return physical_address + kernel_offset;
}

static inline phys_addr_t page_base_address(uint64_t entry_field) {
  return (phys_addr_t)(entry_field << 12);
}

phys_addr_t virtual_to_physical(virt_addr_t virtual_address) {
  const phys_addr_t one_gig = (phys_addr_t)1 << 30;
  const phys_addr_t two_meg = (phys_addr_t)1 << 21;

  if(cr3_vaddr == 0) {
    if(kernel_offset && virtual_address >= kernel_offset) {
      return virtual_address - kernel_offset;
    }
    return PHYS_ADDR_INVALID;
  }

  pml4_t* pml4 = (pml4_t*)cr3_vaddr;
  uint16_t pml4_index = (virtual_address >> 39) & 0x1ff;
  page_map_l4_entry_t pml4_entry = pml4->entries[pml4_index];

  if(!pml4_entry.present || pml4_entry.large_page) {
    return PHYS_ADDR_INVALID;
  }

  page_directory_pointer_t* pdpt =
    (page_directory_pointer_t*)physical_to_virtual(page_base_address(pml4_entry.page_directory_base));

  uint16_t pdpt_index = (virtual_address >> 30) & 0x1ff;
  page_directory_pointer_entry_t pdpt_entry = pdpt->entries[pdpt_index];

  if(!pdpt_entry.present) {
    return PHYS_ADDR_INVALID;
  }

  if(pdpt_entry.large_page) {
    phys_addr_t base = page_base_address(pdpt_entry.page_directory_base);
    return (base & ~(one_gig - 1)) | (virtual_address & (one_gig - 1));
  }

  page_directory_t* pd =
    (page_directory_t*)physical_to_virtual(page_base_address(pdpt_entry.page_directory_base));

  uint16_t pd_index = (virtual_address >> 21) & 0x1ff;
  page_directory_entry_t pd_entry = pd->entries[pd_index];

  if(!pd_entry.present) {
    return PHYS_ADDR_INVALID;
  }

  if(pd_entry.large_page) {
    phys_addr_t base = page_base_address(pd_entry.page_table_base);
    return (base & ~(two_meg - 1)) | (virtual_address & (two_meg - 1));
  }

  page_table_t* pt =
    (page_table_t*)physical_to_virtual(page_base_address(pd_entry.page_table_base));

  uint16_t pt_index = (virtual_address >> 12) & 0x1ff;
  page_table_entry_t pt_entry = pt->entries[pt_index];

  if(!pt_entry.present) {
    return PHYS_ADDR_INVALID;
  }

  phys_addr_t base = page_base_address(pt_entry.frame);
  return (base & ~((phys_addr_t)PAGE_SIZE - 1)) | (virtual_address & (PAGE_SIZE - 1));
}

uintptr_t read_cr2() {
#ifdef __x86_64__
  uintptr_t value;
  asm volatile("movq %%cr2, %0" : "=r" (value));
  return value;
#else
  return 0;
#endif
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
    halt();
  }

  memmap_response = memmap_request.response;
  
  for(uint64_t e = 0; e < memmap_response->entry_count; e++) {
    logk("  %2lu: base: %016lx - size: %10lu - type: %s\n", e, memmap_response->entries[e]->base, 
      memmap_response->entries[e]->length,
      mem_type[memmap_response->entries[e]->type]);

    serial_printf("  Entry %lu:  base: %lx - size: %lu (%lx) - type: %s\n", e, memmap_response->entries[e]->base, 
      memmap_response->entries[e]->length, 
      memmap_response->entries[e]->length, 
      mem_type[memmap_response->entries[e]->type]);

    switch(memmap_response->entries[e]->type) {
    case LIMINE_MEMMAP_USABLE:
      mem_available += memmap_response->entries[e]->length;
      bulk_page_bitmap_as_free(memmap_response->entries[e]->base, memmap_response->entries[e]->length);
      break;
    case LIMINE_MEMMAP_FRAMEBUFFER:
      mem_framebuffer += memmap_response->entries[e]->length;
      break;
    }

    mem_total += memmap_response->entries[e]->length;
  }

  serial_printf("  Total: %luMB - available: %luMB - video: %luMB\n", 
    mem_total / (1024 * 1024), 
    mem_available / (1024 * 1024), 
    mem_framebuffer / (1024 * 1024));

  logk("  Total: %luMB - available: %luMB - video: %luMB\n", 
    mem_total / (1024 * 1024), 
    mem_available / (1024 * 1024), 
    mem_framebuffer / (1024 * 1024));

}

void init_kernel_offset() {
  logk("Getting kernel offset.\n");
  serial_printf("> init_kernel_offset\n");
  kernel_offset = hhdm_request.response->offset;
  serial_printf("  Kernel offset %lx:\n", kernel_offset);
}

void pmm_set_kernel_offset(virt_addr_t offset) {
  kernel_offset = offset;
}

virt_addr_t get_kernel_offset() {
  return kernel_offset;
}

void init_page_bitmap() {
  logk("Initing page bitmap.\n");
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
#ifdef __x86_64__
  phys_addr_t value;
  asm volatile("movq %%cr3, %0" : "=r" (value));
  return value;
#else
  return 0;
#endif
}

void init_cr3() {
  cr3_vaddr = physical_to_virtual(read_cr3());
  logk("CR3 is @ %p\n", cr3_vaddr);
}

void pmm_set_pagetable_root(virt_addr_t root_vaddr) {
  cr3_vaddr = root_vaddr;
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
  logk("Initing Physical memory manager\n");
  serial_puts("\n- Initing Physical memory manager:\n");

  init_page_bitmap();

  list_memory_areas();
  
  init_kernel_offset();

  init_cr3();
}
