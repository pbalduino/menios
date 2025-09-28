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

#endif
