#include <kernel/vm.h>

#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/shm.h>
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
                      phys_frame_t base_frame,
                      size_t page_count,
                      bool writable,
                      bool user,
                      vm_region_t* region) {
  for(size_t page = 0; page < page_count; page++) {
    virt_addr_t vaddr = base + (page * PAGE_SIZE);
    phys_frame_t frame = phys_frame_add(base_frame, page);
    phys_addr_t paddr = phys_frame_to_addr(frame);
    if(!pmm_map_page_in_root(proc->address_space_root, vaddr, frame, writable, user)) {
      for(size_t rollback = 0; rollback < page; rollback++) {
        virt_addr_t r_vaddr = base + (rollback * PAGE_SIZE);
        pmm_unmap_page_in_root(proc->address_space_root, r_vaddr);
      }
      return false;
    }
    if(region != NULL) {
      vm_region_note_mapping(region, vaddr, PAGE_SIZE);
    }
    if(!proc_register_user_segment(proc, paddr, 1)) {
      pmm_unmap_page_in_root(proc->address_space_root, vaddr);
      for(size_t rollback = 0; rollback < page; rollback++) {
        size_t idx = page - rollback - 1;
        virt_addr_t r_vaddr = base + (idx * PAGE_SIZE);
        phys_frame_t r_frame = phys_frame_add(base_frame, idx);
        phys_addr_t r_paddr = phys_frame_to_addr(r_frame);
        pmm_unmap_page_in_root(proc->address_space_root, r_vaddr);
        proc_unregister_user_segment(proc, r_paddr, 1);
      }
      return false;
    }
  }

  return true;
}

bool vm_range_overlaps(proc_info_p proc, virt_addr_t base, size_t length) {
  if(proc == NULL || length == 0) {
    return false;
  }

  virt_addr_t end = base + length;
  for(size_t i = 0; i < proc->vm_region_count; i++) {
    vm_region_t* region = &proc->vm_regions[i];
    virt_addr_t region_base = region->base;
    virt_addr_t region_end = region->base + region->length;
    if(!(end <= region_base || base >= region_end)) {
      return true;
    }
  }
  return false;
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

  phys_frame_t frame = pmm_alloc_pages(pages);
  if(!phys_frame_is_valid(frame)) {
    serial_printf("vm_map: failed to allocate %zu pages\n", pages);
    return false;
  }

  phys_addr_t phys = phys_frame_to_addr(frame);
  memset((void*)physical_to_virtual(phys), 0, pages * PAGE_SIZE);

  if(!map_pages(proc, base, frame, pages, writable, user, region)) {
    pmm_free_pages(frame, pages);
    return false;
  }

  return true;
}

bool vm_map_physical(proc_info_p proc,
                     virt_addr_t base,
                     phys_addr_t phys,
                     size_t length,
                     uint32_t flags) {
  if(proc == NULL || length == 0) {
    return false;
  }

  virt_addr_t aligned_base = align_down(base);
  size_t offset = (size_t)(base - aligned_base);
  phys_addr_t aligned_phys = phys - offset;
  size_t total_length = length + offset;
  size_t aligned_len = page_align_up(total_length);

  if(vm_range_overlaps(proc, aligned_base, aligned_len)) {
    return false;
  }

  if(!vm_region_add(proc, aligned_base, aligned_len, VM_REGION_MMAP, flags)) {
    return false;
  }

  vm_region_t* region = vm_region_find(proc, aligned_base);
  if(region == NULL) {
    if(proc->vm_region_count > 0) {
      proc->vm_region_count--;
    }
    return false;
  }

  bool writable = (flags & VM_REGION_FLAG_WRITE) != 0;
  bool user = (flags & VM_REGION_FLAG_USER) != 0;
  size_t pages = aligned_len / PAGE_SIZE;

  for(size_t page = 0; page < pages; page++) {
    virt_addr_t vaddr = aligned_base + (page * PAGE_SIZE);
    phys_frame_t frame = phys_frame_from_addr(aligned_phys + (page * PAGE_SIZE));
    phys_addr_t paddr = phys_frame_to_addr(frame);
    if(!pmm_map_page_in_root(proc->address_space_root, vaddr, frame, writable, user)) {
      for(size_t rollback = 0; rollback < page; rollback++) {
        virt_addr_t r_vaddr = aligned_base + (rollback * PAGE_SIZE);
        phys_addr_t r_paddr = aligned_phys + (rollback * PAGE_SIZE);
        pmm_unmap_page_in_root(proc->address_space_root, r_vaddr);
        proc_unregister_user_segment(proc, r_paddr, 1);
      }
      if(proc->vm_region_count > 0) {
        proc->vm_region_count--;
      }
      return false;
    }

    if(!proc_register_user_segment(proc, paddr, 1)) {
      pmm_unmap_page_in_root(proc->address_space_root, vaddr);
      for(size_t rollback = 0; rollback < page; rollback++) {
        virt_addr_t r_vaddr = aligned_base + (rollback * PAGE_SIZE);
        phys_addr_t r_paddr = aligned_phys + (rollback * PAGE_SIZE);
        pmm_unmap_page_in_root(proc->address_space_root, r_vaddr);
        proc_unregister_user_segment(proc, r_paddr, 1);
      }
      if(proc->vm_region_count > 0) {
        proc->vm_region_count--;
      }
      return false;
    }

    vm_region_note_mapping(region, vaddr, PAGE_SIZE);
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

bool vm_map_shared(proc_info_p proc,
                   shm_region_t* region,
                   virt_addr_t base,
                   uint32_t flags,
                   bool writable) {
  if(proc == NULL || region == NULL) {
    return false;
  }

  size_t page_count = shm_region_page_count(region);
  if(page_count == 0) {
    return false;
  }

  size_t length = page_count * PAGE_SIZE;
  if(vm_range_overlaps(proc, base, length)) {
    return false;
  }

  if(!vm_region_add(proc, base, length, VM_REGION_SHARED, flags)) {
    return false;
  }

  vm_region_t* vm_reg = vm_region_find(proc, base);
  if(vm_reg == NULL) {
    if(proc->vm_region_count > 0) {
      proc->vm_region_count--;
    }
    return false;
  }

  bool user = (flags & VM_REGION_FLAG_USER) != 0;

  size_t mapped = 0;
  for(; mapped < page_count; ++mapped) {
    virt_addr_t vaddr = base + (mapped * PAGE_SIZE);
    phys_addr_t phys = shm_region_page(region, mapped);
    phys_frame_t frame = phys ? phys_frame_from_addr(phys) : phys_frame_invalid();
    if(!phys_frame_is_valid(frame) ||
       !pmm_map_page_in_root(proc->address_space_root, vaddr, frame, writable, user)) {
      break;
    }
    vm_region_note_mapping(vm_reg, vaddr, PAGE_SIZE);
  }

  if(mapped != page_count) {
    for(size_t i = 0; i < mapped; ++i) {
      virt_addr_t vaddr = base + (i * PAGE_SIZE);
      pmm_unmap_page_in_root(proc->address_space_root, vaddr);
    }
    for(size_t idx = 0; idx < proc->vm_region_count; ++idx) {
      if(&proc->vm_regions[idx] == vm_reg) {
        for(size_t j = idx + 1; j < proc->vm_region_count; ++j) {
          proc->vm_regions[j - 1] = proc->vm_regions[j];
        }
        proc->vm_region_count--;
        break;
      }
    }
    return false;
  }

  vm_reg->committed_base = base;
  vm_reg->committed_top = base + length;
  return true;
}

static bool clone_region(proc_info_p dst,
                         proc_info_p src,
                         vm_region_t* region) {
  if(region->type == VM_REGION_SHARED) {
    return true;
  }

  if(!vm_region_add(dst, region->base, region->length, region->type, region->flags)) {
    return false;
  }

  vm_region_t* dst_region = vm_region_find(dst, region->base);
  if(dst_region == NULL) {
    return false;
  }

  virt_addr_t clone_start = region->committed_base & ~((virt_addr_t)PAGE_SIZE - 1);
  virt_addr_t clone_end = region->committed_top;
  if(clone_end % PAGE_SIZE != 0) {
    clone_end = (clone_end + PAGE_SIZE - 1) & ~((virt_addr_t)PAGE_SIZE - 1);
  }

  if(clone_end <= clone_start) {
    return true;
  }

  size_t pages = (clone_end - clone_start) / PAGE_SIZE;
  bool writable = (region->flags & VM_REGION_FLAG_WRITE) != 0;
  bool user = (region->flags & VM_REGION_FLAG_USER) != 0;

  for(size_t page = 0; page < pages; page++) {
    virt_addr_t vaddr = clone_start + (page * PAGE_SIZE);
    phys_addr_t src_phys;
    if(!pmm_get_mapping(src->address_space_root, vaddr, &src_phys, NULL, NULL)) {
      continue;
    }

    phys_frame_t dst_frame = pmm_alloc_pages(1);
    if(!phys_frame_is_valid(dst_frame)) {
      return false;
    }

    phys_addr_t dst_phys = phys_frame_to_addr(dst_frame);
    memcpy((void*)physical_to_virtual(dst_phys), (void*)physical_to_virtual(src_phys), PAGE_SIZE);

    if(!pmm_map_page_in_root(dst->address_space_root, vaddr, dst_frame, writable, user)) {
      pmm_free_pages(dst_frame, 1);
      return false;
    }

    vm_region_note_mapping(dst_region, vaddr, PAGE_SIZE);
    proc_register_user_segment(dst, dst_phys, 1);
  }

  dst_region->committed_base = region->committed_base;
  dst_region->committed_top = region->committed_top;
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
