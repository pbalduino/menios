#include <kernel/vm.h>

#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <string.h>

static size_t page_align_up(size_t length) {
  if(length == 0) {
    return PAGE_SIZE;
  }
  if(length % PAGE_SIZE == 0) {
    return length;
  }
  return (length / PAGE_SIZE + 1) * PAGE_SIZE;
}

static virt_addr_t align_down(virt_addr_t addr) {
  return addr & ~((virt_addr_t)PAGE_SIZE - 1);
}

static bool map_pages(proc_info_p proc,
                      virt_addr_t base,
                      phys_addr_t phys,
                      size_t page_count,
                      bool writable,
                      bool user,
                      vm_region_t* region) {
  for(size_t page = 0; page < page_count; page++) {
    virt_addr_t vaddr = base + (page * PAGE_SIZE);
    phys_addr_t paddr = phys + (page * PAGE_SIZE);
    if(!pmm_map_page_in_root(proc->address_space_root, vaddr, paddr, writable, user)) {
      for(size_t rollback = 0; rollback < page; rollback++) {
        virt_addr_t r_vaddr = base + (rollback * PAGE_SIZE);
        pmm_unmap_page_in_root(proc->address_space_root, r_vaddr);
      }
      return false;
    }
    if(region != NULL) {
      vm_region_note_mapping(region, vaddr, PAGE_SIZE);
    }
    proc_register_user_segment(proc, paddr, 1);
  }

  return true;
}

bool vm_map(proc_info_p proc, const vm_map_params_t* params) {
  if(proc == NULL || params == NULL || params->length == 0) {
    return false;
  }

  size_t length = page_align_up(params->length);
  virt_addr_t base = align_down(params->base);

  if(!vm_region_add(proc, base, length, params->type, params->flags)) {
    serial_printf("vm_map: failed to register region\n");
    return false;
  }

  vm_region_t* region = vm_region_find(proc, base);
  if(region == NULL) {
    return false;
  }

  size_t pages = length / PAGE_SIZE;
  bool writable = (params->flags & VM_REGION_FLAG_WRITE) != 0;
  bool user = (params->flags & VM_REGION_FLAG_USER) != 0;

  phys_addr_t phys = pmm_alloc_pages(pages);
  if(phys == 0) {
    serial_printf("vm_map: failed to allocate %zu pages\n", pages);
    return false;
  }

  memset((void*)physical_to_virtual(phys), 0, pages * PAGE_SIZE);

  if(!map_pages(proc, base, phys, pages, writable, user, region)) {
    pmm_free_pages(phys, pages);
    return false;
  }

  return true;
}

static bool unmap_page(proc_info_p proc, virt_addr_t vaddr) {
  phys_addr_t frame;
  if(!pmm_get_mapping(proc->address_space_root, vaddr, &frame, NULL, NULL)) {
    return false;
  }

  if(!pmm_unmap_page_in_root(proc->address_space_root, vaddr)) {
    return false;
  }

  proc_unregister_user_segment(proc, frame, 1);
  return true;
}

bool vm_unmap(proc_info_p proc, virt_addr_t base, size_t length) {
  if(proc == NULL || length == 0) {
    return false;
  }

  size_t aligned_len = page_align_up(length);
  virt_addr_t aligned_base = align_down(base);

  vm_region_t* region = vm_region_find(proc, aligned_base);
  if(region == NULL) {
    return false;
  }

  size_t pages = aligned_len / PAGE_SIZE;
  for(size_t page = 0; page < pages; page++) {
    virt_addr_t vaddr = aligned_base + (page * PAGE_SIZE);
    unmap_page(proc, vaddr);
  }

  // Remove region by shifting array entry
  for(size_t i = 0; i < proc->vm_region_count; i++) {
    if(&proc->vm_regions[i] == region) {
      for(size_t j = i + 1; j < proc->vm_region_count; j++) {
        proc->vm_regions[j - 1] = proc->vm_regions[j];
      }
      proc->vm_region_count--;
      break;
    }
  }

  return true;
}

static bool clone_region(proc_info_p dst,
                         proc_info_p src,
                         vm_region_t* region) {
  if(!vm_region_add(dst, region->base, region->length, region->type, region->flags)) {
    return false;
  }

  vm_region_t* dst_region = vm_region_find(dst, region->base);
  if(dst_region == NULL) {
    return false;
  }

  size_t pages = (region->committed_top - region->committed_base) / PAGE_SIZE;
  size_t offset_pages = (region->committed_base - region->base) / PAGE_SIZE;
  bool writable = (region->flags & VM_REGION_FLAG_WRITE) != 0;
  bool user = (region->flags & VM_REGION_FLAG_USER) != 0;

  for(size_t page = 0; page < pages; page++) {
    virt_addr_t vaddr = region->committed_base + (page * PAGE_SIZE);
    phys_addr_t src_phys;
    if(!pmm_get_mapping(src->address_space_root, vaddr, &src_phys, NULL, NULL)) {
      continue;
    }

    phys_addr_t dst_phys = pmm_alloc_pages(1);
    if(dst_phys == 0) {
      return false;
    }

    memcpy((void*)physical_to_virtual(dst_phys), (void*)physical_to_virtual(src_phys), PAGE_SIZE);

    if(!pmm_map_page_in_root(dst->address_space_root, vaddr, dst_phys, writable, user)) {
      pmm_free_pages(dst_phys, 1);
      return false;
    }

    vm_region_note_mapping(dst_region, vaddr, PAGE_SIZE);
    proc_register_user_segment(dst, dst_phys, 1);
  }

  dst_region->committed_base = region->committed_base;
  dst_region->committed_top = region->committed_top;
  (void)offset_pages;
  return true;
}

bool vm_clone(proc_info_p dst, proc_info_p src) {
  if(dst == NULL || src == NULL) {
    return false;
  }

  for(size_t i = 0; i < src->vm_region_count; i++) {
    vm_region_t* region = &src->vm_regions[i];
    if(!clone_region(dst, src, region)) {
      return false;
    }
  }

  return true;
}
