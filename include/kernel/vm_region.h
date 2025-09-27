#ifndef MENIOS_INCLUDE_KERNEL_VM_REGION_H
#define MENIOS_INCLUDE_KERNEL_VM_REGION_H

#include <types.h>

struct proc_info_t;
typedef struct proc_info_t* proc_info_p;

typedef enum vm_region_type_t {
    VM_REGION_TEXT = 0,
    VM_REGION_RODATA,
    VM_REGION_DATA,
    VM_REGION_HEAP,
    VM_REGION_STACK,
    VM_REGION_MMAP
} vm_region_type_t;

#define VM_REGION_FLAG_READ       (1u << 0)
#define VM_REGION_FLAG_WRITE      (1u << 1)
#define VM_REGION_FLAG_EXEC       (1u << 2)
#define VM_REGION_FLAG_USER       (1u << 3)
#define VM_REGION_FLAG_GROW_DOWN  (1u << 4)
#define VM_REGION_FLAG_GROW_UP    (1u << 5)

typedef struct vm_region_t {
    virt_addr_t base;
    size_t      length;
    vm_region_type_t type;
    uint32_t    flags;
    virt_addr_t committed_base;
    virt_addr_t committed_top;
} vm_region_t;

bool vm_region_add(proc_info_p proc,
                   virt_addr_t base,
                   size_t length,
                   vm_region_type_t type,
                   uint32_t flags);

vm_region_t* vm_region_find(proc_info_p proc, virt_addr_t address);

bool vm_region_note_mapping(vm_region_t* region, virt_addr_t base, size_t length);

bool vm_region_handle_page_fault(proc_info_p proc,
                                 virt_addr_t fault_addr,
                                 bool present,
                                 bool is_write,
                                 bool is_user_mode);

#endif
