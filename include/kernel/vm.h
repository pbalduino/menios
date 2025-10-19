#ifndef MENIOS_INCLUDE_KERNEL_VM_H
#define MENIOS_INCLUDE_KERNEL_VM_H

#include <types.h>
#include <kernel/vm_region.h>

struct proc_info_t;
typedef struct proc_info_t* proc_info_p;

typedef struct {
  virt_addr_t base;
  size_t      length;
  uint32_t    flags;
  vm_region_type_t type;
} vm_map_params_t;

bool vm_map(proc_info_p proc, const vm_map_params_t* params);
bool vm_unmap(proc_info_p proc, virt_addr_t base, size_t length);
bool vm_clone(proc_info_p dst, proc_info_p src);
bool vm_range_overlaps(proc_info_p proc, virt_addr_t base, size_t length);
bool vm_map_physical(proc_info_p proc,
                     virt_addr_t base,
                     phys_addr_t phys,
                     size_t length,
                     uint32_t flags);

struct shm_region;
typedef struct shm_region shm_region_t;
bool vm_map_shared(proc_info_p proc,
                   shm_region_t* region,
                   virt_addr_t base,
                   uint32_t flags,
                   bool writable);

#endif
