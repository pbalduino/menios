#include <boot/limine.h>

#include <kernel/console.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void pmm_abort(const char* reason) {
#ifdef MENIOS_KERNEL
  panic("%s", reason);
#else
  fprintf(stderr, "pmm: %s\n", reason);
  abort();
#endif
}

static uint64_t page_bitmap[PAGE_BITMAP_SIZE];
static uintptr_t kernel_offset;
static uintptr_t cr3_vaddr;
static phys_addr_t kernel_cr3_phys = 0;
static size_t pmm_total_usable_pages = 0;
static size_t pmm_free_page_count = 0;
static phys_addr_t pmm_highest_usable_phys = 0;
static phys_addr_t pmm_highest_allowed_phys = 0;
static bool pmm_phys_limits_ready = false;

static inline void invlpg(void* addr) {
#ifdef __x86_64__
  asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
#else
  (void)addr;
#endif
}

static volatile struct limine_hhdm_request hhdm_request = {
  .id = LIMINE_HHDM_REQUEST,
  .revision = 0
};

static volatile struct limine_memmap_request memmap_request = {
  .id = LIMINE_MEMMAP_REQUEST,
  .revision = 0
};

static inline uintptr_t effective_kernel_offset(void) {
  if(kernel_offset != 0) {
    return kernel_offset;
  }
  if(hhdm_request.response != NULL) {
    return hhdm_request.response->offset;
  }
  return 0;
}

static inline phys_addr_t highest_allowed_byte(void) {
  phys_addr_t highest_byte;

  if(!pmm_phys_limits_ready) {
    return UINT64_MAX;
  }

  if(pmm_highest_allowed_phys == 0) {
    return UINT64_MAX;
  }

  if(__builtin_add_overflow(pmm_highest_allowed_phys,
                            (phys_addr_t)PAGE_SIZE - 1,
                            &highest_byte)) {
    return UINT64_MAX;
  }

  return highest_byte;
}

static inline phys_addr_t canonical_to_physical_addr(uintptr_t address,
                                                     uintptr_t offset,
                                                     bool* converted) {
  if(converted) {
    *converted = false;
  }
  if(offset == 0 || address < offset) {
    return (phys_addr_t)address;
  }

  void* ret0 = __builtin_return_address(0);
  void* ret1 = __builtin_return_address(1);

  if(!pmm_phys_limits_ready || pmm_highest_allowed_phys == 0) {
    serial_printf("pmm: HHDM offset=%lx but physical limits unavailable for address %lx\n",
                  (unsigned long)offset,
                  (unsigned long)address);
    serial_printf("pmm: caller ret0=%p ret1=%p\n", ret0, ret1);
    pmm_abort("direct map limits unavailable");
  }

  phys_addr_t phys = (phys_addr_t)(address - offset);
  phys_addr_t highest_byte = highest_allowed_byte();

  if(phys > highest_byte) {
    serial_printf("pmm: rejected virtual address %lx -> phys %lx (highest=%lx offset=%lx)\n",
                  (unsigned long)address,
                  (unsigned long)phys,
                  (unsigned long)highest_byte,
                  (unsigned long)offset);
    serial_printf("pmm: rejection caller ret0=%p ret1=%p\n", ret0, ret1);
    pmm_abort("virtual address outside direct map");
  }

  if(converted) {
    *converted = true;
    serial_printf("pmm: canonicalized address %lx -> phys %lx (ret0=%p ret1=%p)\n",
                  (unsigned long)address,
                  (unsigned long)phys,
                  ret0,
                  ret1);
  }
  return phys;
}

static inline void locate_page_bitmap_slot(const char* caller,
                                           phys_addr_t addr,
                                           size_t* page_number_out,
                                           size_t* index_out,
                                           size_t* bit_out) {
  if((addr % PAGE_SIZE) != 0) {
    serial_printf("%s: unaligned physical address %lx\n",
                  caller,
                  (unsigned long)addr);
    pmm_abort("unaligned page address");
  }

  size_t page_number = (size_t)(addr / PAGE_SIZE);
  size_t index = page_number / (sizeof(uint64_t) * 8);
  size_t bit_position = page_number % (sizeof(uint64_t) * 8);

  if(index >= PAGE_BITMAP_SIZE) {
    serial_printf("%s: physical address %lx out of range (index=%lu page=%lu)\n",
                  caller,
                  (unsigned long)addr,
                  (unsigned long)index,
                  (unsigned long)page_number);
    pmm_abort("physical address outside bitmap coverage");
  }

  if(page_number_out != NULL) {
    *page_number_out = page_number;
  }
  if(index_out != NULL) {
    *index_out = index;
  }
  if(bit_out != NULL) {
    *bit_out = bit_position;
  }
}

phys_addr_t phys_from_hhdm(uintptr_t address) {
  uintptr_t offset = effective_kernel_offset();
  if(offset == 0) {
    serial_printf("phys_from_hhdm: HHDM offset unavailable for address %lx\n",
                  (unsigned long)address);
    pmm_abort("phys_from_hhdm: kernel offset unavailable");
  }

  bool converted = false;
  phys_addr_t phys = canonical_to_physical_addr(address, offset, &converted);
  if(!converted) {
    serial_printf("phys_from_hhdm: address %lx not in HHDM window (offset=%lx)\n",
                  (unsigned long)address,
                  (unsigned long)offset);
    pmm_abort("phys_from_hhdm: address outside HHDM window");
  }

  return phys;
}

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
    uintptr_t offset = effective_kernel_offset();
    if(offset != 0 && virtual_address >= offset) {
      return canonical_to_physical_addr(virtual_address, offset, NULL);
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

void set_page_free(phys_frame_t frame) {
  uintptr_t offset = effective_kernel_offset();
  bool converted = false;
  phys_addr_t addr = canonical_to_physical_addr(phys_frame_to_addr(frame), offset, &converted);
  static bool warned_virtual_input = false;
  if(converted && !warned_virtual_input) {
    serial_printf("set_page_free: converted virtual address %lx using kernel_offset=%lx\n",
                  (unsigned long)phys_frame_to_addr(frame),
                  (unsigned long)offset);
    warned_virtual_input = true;
  }
  size_t index;
  size_t bit_position;
  locate_page_bitmap_slot("set_page_free", addr, NULL, &index, &bit_position);
  uint64_t mask = (1UL << bit_position);
  if(page_bitmap[index] & mask) {
    page_bitmap[index] &= ~mask;
    pmm_free_page_count++;
  }
}

static bool mark_intermediate_user(virt_addr_t vaddr,
                                   page_directory_pointer_t** out_pdpt,
                                   page_directory_entry_t** out_pd_entry,
                                   page_table_entry_t** out_pt_entry) {
  if(cr3_vaddr == 0) {
    return false;
  }

  pml4_t* pml4 = (pml4_t*)cr3_vaddr;
  uint16_t pml4_index = (vaddr >> 39) & 0x1ff;
  page_map_l4_entry_t* pml4_entry = &pml4->entries[pml4_index];
  if(!pml4_entry->present) {
    return false;
  }
  pml4_entry->user = 1;

  page_directory_pointer_t* pdpt = (page_directory_pointer_t*)physical_to_virtual(pml4_entry->page_directory_base << 12);
  uint16_t pdpt_index = (vaddr >> 30) & 0x1ff;
  page_directory_pointer_entry_t* pdpt_entry = &pdpt->entries[pdpt_index];
  if(!pdpt_entry->present) {
    return false;
  }
  pdpt_entry->user = 1;

  if(out_pdpt) {
    *out_pdpt = pdpt;
  }

  if(pdpt_entry->large_page) {
    if(out_pd_entry) {
      *out_pd_entry = NULL;
    }
    if(out_pt_entry) {
      *out_pt_entry = NULL;
    }
    return true;
  }

  page_directory_t* pd = (page_directory_t*)physical_to_virtual(pdpt_entry->page_directory_base << 12);
  uint16_t pd_index = (vaddr >> 21) & 0x1ff;
  page_directory_entry_t* pd_entry = &pd->entries[pd_index];
  if(!pd_entry->present) {
    return false;
  }
  pd_entry->user = 1;

  if(out_pd_entry) {
    *out_pd_entry = pd_entry;
  }

  if(pd_entry->large_page) {
    if(out_pt_entry) {
      *out_pt_entry = NULL;
    }
    return true;
  }

  page_table_t* pt = (page_table_t*)physical_to_virtual(pd_entry->page_table_base << 12);
  uint16_t pt_index = (vaddr >> 12) & 0x1ff;
  page_table_entry_t* pt_entry = &pt->entries[pt_index];
  if(!pt_entry->present) {
    return false;
  }

  if(out_pt_entry) {
    *out_pt_entry = pt_entry;
  }

  return true;
}

static void clear_kernel_user_permissions(phys_addr_t root_phys) {
  if(root_phys == 0) {
    return;
  }

  pml4_t* pml4 = (pml4_t*)physical_to_virtual(root_phys);

  for(size_t pml4_index = 256; pml4_index < 512; pml4_index++) {
    page_map_l4_entry_t* pml4_entry = &pml4->entries[pml4_index];
    if(!pml4_entry->present) {
      continue;
    }

    pml4_entry->user = 0;

    page_directory_pointer_t* pdpt = (page_directory_pointer_t*)physical_to_virtual(pml4_entry->page_directory_base << 12);
    for(size_t pdpt_index = 0; pdpt_index < 512; pdpt_index++) {
      page_directory_pointer_entry_t* pdpt_entry = &pdpt->entries[pdpt_index];
      if(!pdpt_entry->present) {
        continue;
      }

      pdpt_entry->user = 0;
      if(pdpt_entry->large_page) {
        continue;
      }

      page_directory_t* pd = (page_directory_t*)physical_to_virtual(pdpt_entry->page_directory_base << 12);
      for(size_t pd_index = 0; pd_index < 512; pd_index++) {
        page_directory_entry_t* pd_entry = &pd->entries[pd_index];
        if(!pd_entry->present) {
          continue;
        }

        pd_entry->user = 0;
        if(pd_entry->large_page) {
          continue;
        }

        page_table_t* pt = (page_table_t*)physical_to_virtual(pd_entry->page_table_base << 12);
        for(size_t pt_index = 0; pt_index < 512; pt_index++) {
          page_table_entry_t* pt_entry = &pt->entries[pt_index];
          if(!pt_entry->present) {
            continue;
          }
          pt_entry->user = 0;
        }
      }
    }
  }
}

bool pmm_mark_page_user(virt_addr_t vaddr) {
  page_directory_pointer_t* pdpt = NULL;
  page_directory_entry_t* pd_entry = NULL;
  page_table_entry_t* pt_entry = NULL;

  if(!mark_intermediate_user(vaddr, &pdpt, &pd_entry, &pt_entry)) {
    return false;
  }

  if(pt_entry) {
    pt_entry->user = 1;
  }

  invlpg((void*)vaddr);
  return true;
}

bool pmm_mark_range_user(virt_addr_t start, size_t size) {
  if(size == 0) {
    return true;
  }

  virt_addr_t aligned = start & ~((virt_addr_t)PAGE_SIZE - 1);
  virt_addr_t end = start + size;

  for(virt_addr_t addr = aligned; addr < end; addr += PAGE_SIZE) {
    if(!pmm_mark_page_user(addr)) {
      return false;
    }
  }

  return true;
}

bool pmm_map_page_in_root(phys_addr_t root_phys, virt_addr_t vaddr, phys_frame_t frame, bool writable, bool user) {
  phys_addr_t paddr = phys_frame_to_addr(frame);
  if((paddr & (PAGE_SIZE - 1)) != 0 || root_phys == 0 || !phys_frame_is_valid(frame)) {
    return false;
  }

  void* ret0 = __builtin_return_address(0);
  void* ret1 = __builtin_return_address(1);
  serial_printf("pmm_map_page_in_root: root=%lx vaddr=%lx paddr=%lx writable=%d user=%d ret0=%p ret1=%p\n",
                (unsigned long)root_phys,
                (unsigned long)vaddr,
                (unsigned long)paddr,
                writable ? 1 : 0,
                user ? 1 : 0,
                ret0,
                ret1);

  pml4_t* pml4 = (pml4_t*)physical_to_virtual(root_phys);

  uint16_t pml4_index = (vaddr >> 39) & 0x1ff;
  page_map_l4_entry_t* pml4_entry = &pml4->entries[pml4_index];

  if(!pml4_entry->present) {
    phys_frame_t new_pdpt_frame = pmm_alloc_pages(1);
    if(!phys_frame_is_valid(new_pdpt_frame)) {
      return false;
    }
    phys_addr_t new_pdpt_phys = phys_frame_to_addr(new_pdpt_frame);
    memset((void*)physical_to_virtual(new_pdpt_phys), 0, PAGE_SIZE);
    pml4_entry->present = 1;
    pml4_entry->writable = 1;
    pml4_entry->user = user ? 1 : pml4_entry->user;
    pml4_entry->page_directory_base = new_pdpt_phys >> 12;
  }

  if(user) {
    pml4_entry->user = 1;
  }

  page_directory_pointer_t* pdpt = (page_directory_pointer_t*)physical_to_virtual(pml4_entry->page_directory_base << 12);
  uint16_t pdpt_index = (vaddr >> 30) & 0x1ff;
  page_directory_pointer_entry_t* pdpt_entry = &pdpt->entries[pdpt_index];

  if(!pdpt_entry->present) {
    phys_frame_t new_pd_frame = pmm_alloc_pages(1);
    if(!phys_frame_is_valid(new_pd_frame)) {
      return false;
    }
    phys_addr_t new_pd_phys = phys_frame_to_addr(new_pd_frame);
    memset((void*)physical_to_virtual(new_pd_phys), 0, PAGE_SIZE);
    pdpt_entry->present = 1;
    pdpt_entry->writable = 1;
    pdpt_entry->user = user ? 1 : 0;
    pdpt_entry->page_directory_base = new_pd_phys >> 12;
  }

  if(user) {
    pdpt_entry->user = 1;
  }

  if(pdpt_entry->large_page) {
    return false;
  }

  page_directory_t* pd = (page_directory_t*)physical_to_virtual(pdpt_entry->page_directory_base << 12);
  uint16_t pd_index = (vaddr >> 21) & 0x1ff;
  page_directory_entry_t* pd_entry = &pd->entries[pd_index];

  if(!pd_entry->present) {
    phys_frame_t new_pt_frame = pmm_alloc_pages(1);
    if(!phys_frame_is_valid(new_pt_frame)) {
      return false;
    }
    phys_addr_t new_pt_phys = phys_frame_to_addr(new_pt_frame);
    memset((void*)physical_to_virtual(new_pt_phys), 0, PAGE_SIZE);
    pd_entry->present = 1;
    pd_entry->writable = 1;
    pd_entry->user = user ? 1 : 0;
    pd_entry->page_table_base = new_pt_phys >> 12;
  }

  if(user) {
    pd_entry->user = 1;
  }

  if(pd_entry->large_page) {
    return false;
  }

  page_table_t* pt = (page_table_t*)physical_to_virtual(pd_entry->page_table_base << 12);
  uint16_t pt_index = (vaddr >> 12) & 0x1ff;
  page_table_entry_t* pt_entry = &pt->entries[pt_index];

  pt_entry->present = 1;
  pt_entry->writable = writable ? 1 : 0;
  pt_entry->user = user ? 1 : 0;
  pt_entry->write_through = 0;
  pt_entry->cache_disable = 0;
  pt_entry->global = 0;
  pt_entry->available = 0;
  pt_entry->frame = paddr >> 12;
  pt_entry->no_execute = 0;

  invlpg((void*)vaddr);
  return true;
}

bool pmm_map_page(virt_addr_t vaddr, phys_frame_t frame, bool writable, bool user) {
  return pmm_map_page_in_root(read_cr3(), vaddr, frame, writable, user);
}

pml4_walk_result_t pmm_walk_address(phys_addr_t root_phys, virt_addr_t vaddr) {
  pml4_walk_result_t result = {
    .pml4_entry = NULL,
    .pdpt_entry = NULL,
    .pd_entry = NULL,
    .pt_entry = NULL
  };

  if(root_phys == 0) {
    return result;
  }

  pml4_t* pml4 = (pml4_t*)physical_to_virtual(root_phys);
  uint16_t pml4_index = (vaddr >> 39) & 0x1ff;
  page_map_l4_entry_t* pml4_entry = &pml4->entries[pml4_index];
  result.pml4_entry = pml4_entry;
  if(!pml4_entry->present) {
    return result;
  }

  page_directory_pointer_t* pdpt = (page_directory_pointer_t*)physical_to_virtual(pml4_entry->page_directory_base << 12);
  uint16_t pdpt_index = (vaddr >> 30) & 0x1ff;
  page_directory_pointer_entry_t* pdpt_entry = &pdpt->entries[pdpt_index];
  result.pdpt_entry = pdpt_entry;
  if(!pdpt_entry->present || pdpt_entry->large_page) {
    return result;
  }

  page_directory_t* pd = (page_directory_t*)physical_to_virtual(pdpt_entry->page_directory_base << 12);
  uint16_t pd_index = (vaddr >> 21) & 0x1ff;
  page_directory_entry_t* pd_entry = &pd->entries[pd_index];
  result.pd_entry = pd_entry;
  if(!pd_entry->present || pd_entry->large_page) {
    return result;
  }

  page_table_t* pt = (page_table_t*)physical_to_virtual(pd_entry->page_table_base << 12);
  uint16_t pt_index = (vaddr >> 12) & 0x1ff;
  page_table_entry_t* pt_entry = &pt->entries[pt_index];
  result.pt_entry = pt_entry;

  return result;
}

static bool pt_has_present_entries(const page_table_t* pt) {
  if(pt == NULL) {
    return false;
  }
  for(size_t i = 0; i < 512; i++) {
    if(pt->entries[i].present) {
      return true;
    }
  }
  return false;
}

static bool pd_has_present_entries(const page_directory_t* pd) {
  if(pd == NULL) {
    return false;
  }
  for(size_t i = 0; i < 512; i++) {
    if(pd->entries[i].present) {
      return true;
    }
  }
  return false;
}

static bool pdpt_has_present_entries(const page_directory_pointer_t* pdpt) {
  if(pdpt == NULL) {
    return false;
  }
  for(size_t i = 0; i < 512; i++) {
    if(pdpt->entries[i].present) {
      return true;
    }
  }
  return false;
}

static bool pmm_unmap_page_internal(phys_addr_t root_phys,
                                    virt_addr_t vaddr,
                                    bool free_frame) {
  if(root_phys == 0) {
    return false;
  }

  void* ret0 = __builtin_return_address(0);
  void* ret1 = __builtin_return_address(1);
  serial_printf("pmm_unmap_page_internal: root=%lx vaddr=%lx free=%d ret0=%p ret1=%p\n",
                (unsigned long)root_phys,
                (unsigned long)vaddr,
                free_frame ? 1 : 0,
                ret0,
                ret1);

  pml4_walk_result_t walk = pmm_walk_address(root_phys, vaddr);
  if(walk.pt_entry == NULL || !walk.pt_entry->present) {
    return false;
  }

  walk.pt_entry->present = 0;
  phys_addr_t frame = walk.pt_entry->frame << 12;
  walk.pt_entry->frame = 0;
  invlpg((void*)vaddr);

  if(walk.pd_entry) {
    page_table_t* pt = (page_table_t*)physical_to_virtual(walk.pd_entry->page_table_base << 12);
    if(!pt_has_present_entries(pt)) {
      phys_addr_t pt_phys = walk.pd_entry->page_table_base << 12;
      walk.pd_entry->present = 0;
      walk.pd_entry->page_table_base = 0;
      pmm_free_pages(phys_frame_from_addr(pt_phys), 1);
    }
  }

  if(walk.pdpt_entry) {
    page_directory_t* pd = (page_directory_t*)physical_to_virtual(walk.pdpt_entry->page_directory_base << 12);
    if(!pd_has_present_entries(pd)) {
      phys_addr_t pd_phys = walk.pdpt_entry->page_directory_base << 12;
      walk.pdpt_entry->present = 0;
      walk.pdpt_entry->page_directory_base = 0;
      pmm_free_pages(phys_frame_from_addr(pd_phys), 1);
    }
  }

  if(walk.pml4_entry) {
    page_directory_pointer_t* pdpt = (page_directory_pointer_t*)physical_to_virtual(walk.pml4_entry->page_directory_base << 12);
    if(!pdpt_has_present_entries(pdpt)) {
      phys_addr_t pdpt_phys = walk.pml4_entry->page_directory_base << 12;
      walk.pml4_entry->present = 0;
      walk.pml4_entry->page_directory_base = 0;
      pmm_free_pages(phys_frame_from_addr(pdpt_phys), 1);
    }
  }

  if(free_frame) {
    pmm_free_pages(phys_frame_from_addr(frame), 1);
  }
  return true;
}

bool pmm_unmap_page_in_root(phys_addr_t root_phys, virt_addr_t vaddr) {
  return pmm_unmap_page_internal(root_phys, vaddr, true);
}

bool pmm_remove_mapping_in_root(phys_addr_t root_phys, virt_addr_t vaddr) {
  return pmm_unmap_page_internal(root_phys, vaddr, false);
}

bool pmm_get_mapping(phys_addr_t root_phys, virt_addr_t vaddr, phys_addr_t* out_phys, bool* out_writable, bool* out_user) {
  if(root_phys == 0) {
    return false;
  }

  pml4_walk_result_t walk = pmm_walk_address(root_phys, vaddr);
  if(walk.pt_entry == NULL || !walk.pt_entry->present) {
    return false;
  }

  if(out_phys) {
    *out_phys = walk.pt_entry->frame << 12;
  }
  if(out_writable) {
    *out_writable = walk.pt_entry->writable != 0;
  }
  if(out_user) {
    *out_user = walk.pt_entry->user != 0;
  }

  return true;
}

phys_addr_t pmm_clone_kernel_address_space(void) {
  phys_addr_t root_phys = kernel_cr3_phys ? kernel_cr3_phys : read_cr3();
  phys_frame_t new_root_frame = pmm_alloc_pages(1);
  if(!phys_frame_is_valid(new_root_frame)) {
    return 0;
  }

  phys_addr_t new_root = phys_frame_to_addr(new_root_frame);

  void* src = (void*)physical_to_virtual(root_phys);
  void* dst = (void*)physical_to_virtual(new_root);
  memcpy(dst, src, PAGE_SIZE);

  memset(dst, 0, sizeof(page_map_l4_entry_t) * 256);

  clear_kernel_user_permissions(new_root);

  return new_root;
}

phys_addr_t pmm_get_kernel_cr3(void) {
  return kernel_cr3_phys;
}

void set_page_used(phys_frame_t frame) {
  uintptr_t offset = effective_kernel_offset();
  bool converted = false;
  phys_addr_t addr = canonical_to_physical_addr(phys_frame_to_addr(frame), offset, &converted);
  static bool warned_virtual_input = false;
  if(converted && !warned_virtual_input) {
    serial_printf("set_page_used: converted virtual address %lx using kernel_offset=%lx\n",
                  (unsigned long)phys_frame_to_addr(frame),
                  (unsigned long)offset);
    warned_virtual_input = true;
  }
  size_t index;
  size_t bit_position;
  locate_page_bitmap_slot("set_page_used", addr, NULL, &index, &bit_position);
  uint64_t mask = (1UL << bit_position);
  if((page_bitmap[index] & mask) == 0) {
    page_bitmap[index] |= mask;
    if(pmm_free_page_count > 0) {
      pmm_free_page_count--;
    }
  }
}

void set_page_row_free(phys_frame_t frame) {
  uintptr_t offset = effective_kernel_offset();
  phys_addr_t addr = canonical_to_physical_addr(phys_frame_to_addr(frame), offset, NULL);
  size_t page_number;
  size_t index;
  size_t bit_position;
  locate_page_bitmap_slot("set_page_row_free", addr, &page_number, &index, &bit_position);
  if(bit_position != 0) {
    serial_printf("set_page_row_free: base %lx not row-aligned (page=%lu)\n",
                  (unsigned long)addr,
                  (unsigned long)page_number);
    pmm_abort("row free base not aligned");
  }
  uint64_t previous = page_bitmap[index];
  if(previous != PAGE_FREE) {
    size_t used = (size_t)__builtin_popcountll(previous);
    if(used < 64) {
      pmm_free_page_count += (64 - used);
    }
    page_bitmap[index] = PAGE_FREE;
  }
}

static phys_frame_t pmm_alloc_internal(size_t page_count,
                                       size_t alignment,
                                       phys_addr_t max_phys_addr) {
  if(page_count == 0) {
    return phys_frame_invalid();
  }

  if(alignment < PAGE_SIZE) {
    alignment = PAGE_SIZE;
  }

  size_t alignment_pages = (alignment + PAGE_SIZE - 1) / PAGE_SIZE;
  if(alignment_pages == 0) {
    alignment_pages = 1;
  }

  size_t run_length = 0;
  size_t run_start = 0;

  for(size_t index = 0; index < PAGE_BITMAP_SIZE; index++) {
    uint64_t row = page_bitmap[index];

    if(row == PAGE_BITMAP_FULL) {
      run_length = 0;
      continue;
    }

    for(size_t bit = 0; bit < 64; bit++) {
      size_t page_number = (index * 64) + bit;

      if((row & (1UL << bit)) == 0) {
        if(run_length == 0) {
          run_start = page_number;
        }

        run_length++;

        if(run_length >= page_count) {
          size_t run_end = page_number + 1; // exclusive
          size_t candidate = run_start;
          if(alignment_pages > 1) {
            size_t aligned = ((run_start + alignment_pages - 1) / alignment_pages) * alignment_pages;
            if(aligned + page_count <= run_end) {
              candidate = aligned;
            } else {
              // alignment pushes beyond current run; keep extended run
              continue;
            }
          }

          phys_addr_t base = (phys_addr_t)candidate * PAGE_SIZE;
          phys_addr_t span = (phys_addr_t)page_count * PAGE_SIZE;
          if(span == 0) {
            return phys_frame_invalid();
          }
          uintptr_t offset = effective_kernel_offset();
          if(offset != 0 && base >= offset) {
            serial_printf("pmm_alloc_internal: candidate produced virtual base=%lx (candidate=%lu index=%lu bit=%lu)\n",
                          (unsigned long)base,
                          (unsigned long)candidate,
                          (unsigned long)index,
                          (unsigned long)bit);
            return phys_frame_invalid();
          }
          if(base > max_phys_addr || (span - 1) > (max_phys_addr - base)) {
            continue;
          }

          for(size_t page = 0; page < page_count; page++) {
            set_page_used(phys_frame_from_addr(base + (page * PAGE_SIZE)));
          }

          return phys_frame_from_addr(base);
        }
      } else {
        run_length = 0;
      }
    }
  }

  return phys_frame_invalid();
}

phys_frame_t pmm_alloc_pages(size_t page_count) {
  phys_addr_t limit = pmm_highest_usable_phys;
  if(limit == 0) {
    limit = UINT64_MAX;
  }
  phys_frame_t base = pmm_alloc_internal(page_count, PAGE_SIZE, limit);
  serial_printf("pmm_alloc_pages: count=%lu -> base=%lx\n",
                (unsigned long)page_count,
                (unsigned long)(phys_frame_is_valid(base) ? phys_frame_to_addr(base) : 0UL));
  return base;
}

phys_frame_t pmm_alloc_aligned_pages(size_t page_count,
                                     size_t alignment,
                                     phys_addr_t max_phys_addr) {
  phys_addr_t limit = pmm_highest_usable_phys;
  if(limit == 0 || (max_phys_addr != UINT64_MAX && max_phys_addr < limit)) {
    limit = max_phys_addr;
  }
  if(limit == 0) {
    limit = UINT64_MAX;
  }
  return pmm_alloc_internal(page_count, alignment, limit);
}

void pmm_free_pages(phys_frame_t base_frame, size_t page_count) {
  serial_printf("pmm_free_pages: base=%lx count=%lu\n",
                (unsigned long)(phys_frame_is_valid(base_frame) ? phys_frame_to_addr(base_frame) : 0UL),
                (unsigned long)page_count);
  if(page_count == 0) {
    return;
  }
  if(!phys_frame_is_valid(base_frame)) {
    serial_printf("pmm_free_pages: invalid base frame\n");
    return;
  }

  uintptr_t offset = effective_kernel_offset();
  bool converted = false;
  phys_addr_t phys_base =
    canonical_to_physical_addr(phys_frame_is_valid(base_frame) ? phys_frame_to_addr(base_frame) : PHYS_ADDR_INVALID,
                               offset,
                               &converted);
  if(phys_base == PHYS_ADDR_INVALID) {
    return;
  }
  phys_frame_t phys_base_frame = phys_frame_from_addr(phys_base);
  if(converted) {
    serial_printf("pmm_free_pages: converted virtual base=%lx using kernel_offset=%lx\n",
                  (unsigned long)phys_frame_to_addr(base_frame),
                  (unsigned long)offset);
  }
  for(size_t page = 0; page < page_count; page++) {
    set_page_free(phys_frame_add(phys_base_frame, page));
  }
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
    pmm_total_usable_pages += (size_t)(size / PAGE_SIZE);
    phys_addr_t region_end = base + size;
    if(region_end >= PAGE_SIZE) {
      phys_addr_t last_page_phys = region_end - PAGE_SIZE;
      if(last_page_phys > pmm_highest_usable_phys) {
        pmm_highest_usable_phys = last_page_phys;
      }
      if(last_page_phys > pmm_highest_allowed_phys) {
        pmm_highest_allowed_phys = last_page_phys;
      }
      pmm_phys_limits_ready = true;
    }
    for(uint64_t p = base; p < base + size; p += PAGE_SIZE) {
      uint64_t b = p - base;
      uint64_t r = size - b;

      if(p % PAGE_ROW_SIZE == 0 && r > PAGE_ROW_SIZE) {
        set_page_row_free(phys_frame_from_addr(p));
        p += (PAGE_ROW_SIZE - PAGE_SIZE);
      }

      set_page_free(phys_frame_from_addr(p));
    }
  } else {
    serial_printf("bulk_page_bitmap_as_free what?\n");
  }
}

void list_memory_areas() {
  uint64_t mem_total = 0;
  uint64_t mem_available = 0;
  uint64_t mem_framebuffer = 0;

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

    uint64_t entry_base = memmap_response->entries[e]->base;
    uint64_t entry_size = memmap_response->entries[e]->length;
    uint64_t entry_end = entry_base + entry_size;
    if(entry_end >= PAGE_SIZE) {
      phys_addr_t last_page_phys = (phys_addr_t)(entry_end - PAGE_SIZE);
      if(last_page_phys > pmm_highest_allowed_phys) {
        pmm_highest_allowed_phys = last_page_phys;
        pmm_phys_limits_ready = true;
      }
    }

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

void pmm_get_stats(pmm_stats_t* stats) {
  if(stats == NULL) {
    return;
  }
  stats->usable_pages = pmm_total_usable_pages;
  stats->free_pages = pmm_free_page_count;
}

void pmm_kernel_offset_initialize(void) {
  logk("Getting kernel offset.\n");
  serial_printf("> pmm_kernel_offset_initialize\n");
  kernel_offset = hhdm_request.response->offset;
  serial_printf("  Kernel offset %lx:\n", kernel_offset);
}

void pmm_set_kernel_offset(virt_addr_t offset) {
  kernel_offset = offset;
}

virt_addr_t get_kernel_offset() {
  return kernel_offset;
}

void pmm_page_bitmap_initialize(void) {
  logk("Initing page bitmap.\n");
  memsetl(&page_bitmap[0], PAGE_BITMAP_FULL, PAGE_BITMAP_SIZE);
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

void pmm_cr3_initialize(void) {
  kernel_cr3_phys = read_cr3();
  cr3_vaddr = physical_to_virtual(kernel_cr3_phys);
  clear_kernel_user_permissions(kernel_cr3_phys);
  logk("CR3 is @ %p\n", cr3_vaddr);
}

void write_cr3(phys_addr_t value) {
#ifdef __x86_64__
  asm volatile("mov %0, %%cr3" :: "r"(value) : "memory");
#endif
  cr3_vaddr = physical_to_virtual(value);
}

void pmm_set_pagetable_root(virt_addr_t root_vaddr) {
  cr3_vaddr = root_vaddr;
}

void pml4_map(uintptr_t vaddr, pml4_map_t* map) {
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
void pmm_initialize(void) {
  logk("Initing Physical memory manager\n");
  serial_puts("\n- Initing Physical memory manager:\n");

  pmm_page_bitmap_initialize();

  list_memory_areas();
  
  pmm_kernel_offset_initialize();

  pmm_cr3_initialize();
}
