#include <kernel/vm_region.h>

#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <string.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

static inline virt_addr_t page_align_down(virt_addr_t addr) {
    return addr & ~((virt_addr_t)PAGE_SIZE - 1);
}

static inline virt_addr_t region_limit(const vm_region_t* region) {
    return region->base + region->length;
}

bool vm_region_add(proc_info_p proc,
                   virt_addr_t base,
                   size_t length,
                   vm_region_type_t type,
                   uint32_t flags) {
    if(proc == NULL || length == 0 || proc->vm_region_count >= PROC_MAX_VM_REGIONS) {
        return false;
    }

    vm_region_t* region = &proc->vm_regions[proc->vm_region_count++];
    region->base = base;
    region->length = length;
    region->type = type;
    region->flags = flags;

    if(flags & VM_REGION_FLAG_GROW_DOWN) {
        region->committed_base = region_limit(region);
        region->committed_top = region_limit(region);
    } else {
        region->committed_base = base;
        region->committed_top = base;
    }

    return true;
}

vm_region_t* vm_region_find(proc_info_p proc, virt_addr_t address) {
    if(proc == NULL) {
        return NULL;
    }

    for(size_t i = 0; i < proc->vm_region_count; i++) {
        vm_region_t* region = &proc->vm_regions[i];
        virt_addr_t limit = region_limit(region);
        if(address >= region->base && address < limit) {
            return region;
        }
    }

    return NULL;
}

bool vm_region_note_mapping(vm_region_t* region, virt_addr_t base, size_t length) {
    if(region == NULL || length == 0) {
        return false;
    }

    virt_addr_t end = base + length;

    if(region->flags & VM_REGION_FLAG_GROW_DOWN) {
        if(base < region->committed_base) {
            region->committed_base = base;
        }
        if(end > region->committed_top) {
            region->committed_top = end;
        }
    } else {
        if(base < region->committed_base) {
            region->committed_base = base;
        }
        if(end > region->committed_top) {
            region->committed_top = end;
        }
    }

    return true;
}

static bool vm_region_grow_down(proc_info_p proc,
                                vm_region_t* region,
                                virt_addr_t fault_addr) {
    virt_addr_t aligned = page_align_down(fault_addr);

    if(aligned < region->base) {
        return false;
    }

    if(region->committed_base <= aligned) {
        return true; // Already committed
    }

    phys_addr_t phys = pmm_alloc_pages(1);
    if(phys == 0) {
        serial_printf("vm_region_grow_down: out of physical memory\n");
        return false;
    }

    void* page_ptr = (void*)physical_to_virtual(phys);
    memset(page_ptr, 0, PAGE_SIZE);

    bool writable = (region->flags & VM_REGION_FLAG_WRITE) != 0;
    if(!pmm_map_page_in_root(proc->address_space_root, aligned, phys, writable, true)) {
        serial_printf("vm_region_grow_down: map failed at %lx\n", aligned);
        pmm_free_pages(phys, 1);
        return false;
    }

    if(!proc_register_user_segment(proc, phys, 1)) {
        serial_printf("vm_region_grow_down: segment registration failed\n");
        // Unmap and free page
        pmm_unmap_page_in_root(proc->address_space_root, aligned);
        pmm_free_pages(phys, 1);
        return false;
    }

    vm_region_note_mapping(region, aligned, PAGE_SIZE);
    return true;
}

static bool vm_region_grow_up(proc_info_p proc,
                              vm_region_t* region,
                              virt_addr_t fault_addr) {
    virt_addr_t aligned = page_align_down(fault_addr);
    virt_addr_t limit = region_limit(region);

    if(aligned >= limit) {
        return false;
    }

    if(region->committed_top > aligned) {
        return true;
    }

    phys_addr_t phys = pmm_alloc_pages(1);
    if(phys == 0) {
        serial_printf("vm_region_grow_up: out of physical memory\n");
        return false;
    }

    void* page_ptr = (void*)physical_to_virtual(phys);
    memset(page_ptr, 0, PAGE_SIZE);

    bool writable = (region->flags & VM_REGION_FLAG_WRITE) != 0;
    if(!pmm_map_page_in_root(proc->address_space_root, aligned, phys, writable, true)) {
        serial_printf("vm_region_grow_up: map failed at %lx\n", aligned);
        pmm_free_pages(phys, 1);
        return false;
    }

    if(!proc_register_user_segment(proc, phys, 1)) {
        serial_printf("vm_region_grow_up: segment registration failed\n");
        pmm_unmap_page_in_root(proc->address_space_root, aligned);
        pmm_free_pages(phys, 1);
        return false;
    }

    vm_region_note_mapping(region, aligned, PAGE_SIZE);
    return true;
}

bool vm_region_handle_page_fault(proc_info_p proc,
                                 virt_addr_t fault_addr,
                                 bool present,
                                 bool is_write,
                                 bool is_user_mode) {
    if(proc == NULL || present || !is_user_mode) {
        return false;
    }

    vm_region_t* region = vm_region_find(proc, fault_addr);
    if(region == NULL) {
        return false;
    }

    if(is_write && !(region->flags & VM_REGION_FLAG_WRITE)) {
        return false;
    }

    if(region->flags & VM_REGION_FLAG_GROW_DOWN) {
        return vm_region_grow_down(proc, region, fault_addr);
    }

    if(region->flags & VM_REGION_FLAG_GROW_UP) {
        return vm_region_grow_up(proc, region, fault_addr);
    }

    return false;
}
