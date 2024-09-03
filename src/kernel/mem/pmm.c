#include <boot/limine.h>
#include <kernel/kheap.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <stdio.h>
#include <mem.h>

static uint64_t page_bitmap[PAGE_BITMAP_SIZE];
static uintptr_t kernel_offset;
static uintptr_t heap_offset;

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

static inline uintptr_t physical_to_virtual(uintptr_t physical_address) {
  // serial_printf("p2v: %lx + %lx = %lx\n", physical_address, kernel_offset, physical_address - kernel_offset);
  return physical_address + kernel_offset;
}

static inline uintptr_t virtual_to_physical(uintptr_t virtual_address) {
  // serial_printf("v2p: %lx - %lx = %lx\n", virtual_address, kernel_offset, virtual_address - kernel_offset);
  return virtual_address - kernel_offset;
}

uint64_t read_cr2() {
  uint64_t value;
  asm volatile("movq %%cr2, %0" : "=r" (value));
  return value;
}

void force_page_fault() {
  uint8_t *y = (uint8_t*)0x7f0080000000;
  // *y = 

  y[0] = 'A';
  y[1] = 'B';
  y[2] = 'C';
  y[3] = 'D';

  printf("y: %x\n", (unsigned long)y);
  printf("y.str = %s\n", y);
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
  size_t bit_position = page_number % (sizeof(unsigned long) * 8);
  page_bitmap[index] = PAGE_FREE;
}

uint8_t get_page_status(uintptr_t physical_address) {
  size_t page_number = physical_address / PAGE_SIZE;
  size_t index = page_number / (sizeof(uint64_t) * 8);
  size_t bit_position = page_number % (sizeof(uint64_t) * 8);
  
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

  if (memmap_request.response == NULL) {
    printf("== error reading limine_memmap_response ==\n");
    serial_error("== error reading limine_memmap_response ==\n");
    hcf();
  }

   memmap_response = memmap_request.response;
  
  for(uint64_t e = 0; e < memmap_response->entry_count; e++) {
    printf("  Entry %lu:  base: %lx - size: %lu (%lx) - type: %s\n", e, memmap_response->entries[e]->base, memmap_response->entries[e]->length, memmap_response->entries[e]->length, mem_type[memmap_response->entries[e]->type]);
    serial_printf("  Entry %lu:  base: %lx - size: %lu (%lx) - type: %s\n", e, memmap_response->entries[e]->base, memmap_response->entries[e]->length, memmap_response->entries[e]->length, mem_type[memmap_response->entries[e]->type]);
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

  serial_printf("  Total: %luMB - available: %luMB - video: %luMB\n", mem_total / (1024 * 1024), mem_available / (1024 * 1024), mem_framebuffer / (1024 * 1024));
}

void init_kernel_offset() {
  serial_printf("> init_kernel_offset\n");
  kernel_offset = hhdm_request.response->offset;
  printf("  Kernel offset %lx:\n", kernel_offset);
  serial_printf("  Kernel offset %lx:\n", kernel_offset);
}

void init_page_bitmap() {
  memsetl(&page_bitmap, PAGE_BITMAP_FULL, PAGE_BITMAP_SIZE);
}

uint64_t get_first_free_page() {
for(size_t p = 0; p < PAGE_BITMAP_SIZE; p++) {
    uint64_t page_row = page_bitmap[p];
    if(page_row != PAGE_BITMAP_FULL) {
      for(int b = 0; b < 64; b++) {
        if((page_row & (1UL << b)) == 0) {
          serial_printf("get_first_free_page: first free page: %d - %lx - %lx - %lx\n", b, p * PAGE_ROW_SIZE, page_row, (p * PAGE_ROW_SIZE) + (b * PAGE_SIZE));
          return (p * PAGE_ROW_SIZE) + (b * PAGE_SIZE);
        }
      }
    }
  }

  return 0L;
}

uint64_t read_cr3() {
  uint64_t value;
  asm volatile("movq %%cr3, %0" : "=r" (value));
  return value;
}

void invalidate_pages_from_cr3() {
  serial_printf("> invalidate_pages_from_cr3\n");

  uint64_t cr3_physaddr = read_cr3();
  serial_printf("> invalidate_pages_from_cr3 - cr3: %lx - %lx\n", cr3_physaddr, physical_to_virtual(cr3_physaddr));

  page_map_l4_entry_t *entry = (page_map_l4_entry_t *)physical_to_virtual(cr3_physaddr);

  uint64_t pdbase;

  serial_printf("pml4 offset: %lx\n", (kernel_offset >> 39) & 0x1ff);

  for(int h = 0x100; h < 512; h++) {
    if(entry[h].present == 1 && entry[h].writable) {
      serial_printf("pe: %lx(%d) @ %lx\n", h, entry[h].present, entry[h].page_directory_base);
      pdbase = entry[h].page_directory_base;

      page_directory_pointer_t *pdp = (page_directory_pointer_t *)physical_to_virtual(pdbase * 0x1000);

      for (int i = 0; i < 512; i++) {
        if(pdp->entries[i].present == 1 && pdp->entries[i].writable) {
          serial_printf("  pd: %lx(%d) @ %lx\n", i, pdp->entries[i].present, pdp->entries[i].page_directory_base);
          page_directory_t *pd = (page_directory_t *)physical_to_virtual(pdp->entries[i].page_directory_base * 0x1000);

          for (int j = 0; j < 512; j++) {
            if(pd->entries[j].present == 1 && pd->entries[j].writable) {
              serial_printf("    pt: %lx(%d) @ %lx\n", j, pd->entries[j].present, pd->entries[j].page_table_base);
              page_table_t *pt = (page_table_t *)physical_to_virtual(pd->entries[j].page_table_base * 0x1000);

              for (int k = 0; k < 512; k++) {
                if(pt->entries[k].present == 0) {
                  serial_printf("      p: %lx(%d) @ %lx\n", k, pt->entries[k].present, pt->entries[k].frame);
                }
              }
            }
          }
        }
      }
    }
  }
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
  printf("- Initing Physical memory manager:\n");
  serial_puts("\n- Initing Physical memory manager:\n");

  init_page_bitmap();

  init_kernel_offset();

  list_memory_areas();

  // invalidate_pages_from_cr3();

  for(uint64_t p = 0; p < PAGE_BITMAP_SIZE; p++) {
    if(page_bitmap[p] != PAGE_BITMAP_FULL) {
      serial_printf("%lx: %lx\n", p * PAGE_ROW_SIZE, page_bitmap[p]);
    }
  }

  for(int i = 0; i < 100; i++) {
    uint64_t m = get_first_free_page();
    set_page_used(m);
  }

  get_first_free_page();

  kmalloc(1);

  force_page_fault();
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