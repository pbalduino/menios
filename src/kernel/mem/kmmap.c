#include <errno.h>
#include <kernel/mman.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/vm.h>
#include <kernel/vm_region.h>
#include <kernel/pmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <types.h>

#define MMAP_SUPPORTED_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)

static size_t page_align_up_size(size_t value) {
  if(value == 0) {
    return PAGE_SIZE;
  }
  size_t remainder = value % PAGE_SIZE;
  if(remainder == 0) {
    return value;
  }
  return value + (PAGE_SIZE - remainder);
}

static virt_addr_t page_align_down_addr(virt_addr_t value) {
  return value & ~((virt_addr_t)PAGE_SIZE - 1);
}

static virt_addr_t page_align_up_addr(virt_addr_t value) {
  if((value & (PAGE_SIZE - 1)) == 0) {
    return value;
  }
  return (value + PAGE_SIZE) & ~((virt_addr_t)PAGE_SIZE - 1);
}

static bool mmap_flags_supported(int flags) {
  if((flags & MAP_ANONYMOUS) == 0) {
    return false;
  }
  if(flags & ~MMAP_SUPPORTED_FLAGS) {
    return false;
  }
  return true;
}

static uint32_t prot_to_region_flags(int prot) {
  uint32_t flags = VM_REGION_FLAG_USER;
  if(prot & PROT_READ) {
    flags |= VM_REGION_FLAG_READ;
  }
  if(prot & PROT_WRITE) {
    flags |= VM_REGION_FLAG_WRITE;
  }
  if(prot & PROT_EXEC) {
    flags |= VM_REGION_FLAG_EXEC;
  }
  return flags;
}

static bool check_overflow(virt_addr_t base, size_t length) {
  return length > (size_t)(UINT64_MAX - base);
}

void* kmmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  (void)fd;
  (void)offset;

  if(current == NULL || !current->user_mode) {
    if(current) {
      current->err_no = ENOSYS;
    }
    return MAP_FAILED;
  }

  if(!mmap_flags_supported(flags)) {
    current->err_no = EINVAL;
    return MAP_FAILED;
  }

  size_t aligned_len = page_align_up_size(length);
  if(check_overflow(0, aligned_len)) {
    current->err_no = EINVAL;
    return MAP_FAILED;
  }

  virt_addr_t base_hint = current->mmap_next ? current->mmap_next : current->mmap_base;
  serial_printf("kmmap: pid=%u base_hint=%lx mmap_base=%lx mmap_next=%lx len=%lu hint=%p flags=%x\n",
                current->pid,
                (unsigned long)base_hint,
                (unsigned long)current->mmap_base,
                (unsigned long)current->mmap_next,
                (unsigned long)aligned_len,
                addr,
                flags);
  if(base_hint < current->mmap_base || base_hint >= current->mmap_limit) {
    base_hint = current->mmap_base;
    current->mmap_next = current->mmap_base;
  }

  virt_addr_t base = addr ? page_align_down_addr((virt_addr_t)addr)
                          : page_align_up_addr(base_hint);

  bool hint = (addr != NULL);

  if(check_overflow(base, aligned_len)) {
    current->err_no = EINVAL;
    return MAP_FAILED;
  }

  virt_addr_t end = base + aligned_len;

  if(base < current->mmap_base || end > current->mmap_limit || base >= end) {
    current->err_no = ENOMEM;
    return MAP_FAILED;
  }

  if(!hint) {
    while(end <= current->mmap_limit && vm_range_overlaps(current, base, aligned_len)) {
      base = page_align_up_addr(end);
      if(check_overflow(base, aligned_len)) {
        current->err_no = ENOMEM;
        return MAP_FAILED;
      }
      end = base + aligned_len;
    }

    if(end > current->mmap_limit || base >= end) {
      current->err_no = ENOMEM;
      return MAP_FAILED;
    }
  } else if(vm_range_overlaps(current, base, aligned_len)) {
    current->err_no = EINVAL;
    return MAP_FAILED;
  }

  vm_map_params_t params = {
    .base = base,
    .length = aligned_len,
    .flags = prot_to_region_flags(prot),
    .type = VM_REGION_MMAP
  };

  if(!vm_map(current, &params)) {
    current->err_no = ENOMEM;
    return MAP_FAILED;
  }

  if(!hint) {
    virt_addr_t next = page_align_up_addr(end);
    if(next > current->mmap_next) {
      current->mmap_next = next;
    }
  }

  serial_printf("kmmap: pid=%u returning base=%lx len=%lu\n",
                current->pid,
                (unsigned long)base,
                (unsigned long)aligned_len);

  if(base == 0) {
    serial_printf("kmmap: refusing to return NULL mapping (len=%lu) base_hint=%lx\n",
                  (unsigned long)aligned_len,
                  (unsigned long)base_hint);
    vm_unmap(current, base, aligned_len);
    current->err_no = ENOMEM;
    return MAP_FAILED;
  }

  current->err_no = 0;
  return (void*)base;
}

int kmunmap(void *addr, size_t len) {
  if(current == NULL || !current->user_mode || addr == NULL || len == 0) {
    if(current) {
      current->err_no = EINVAL;
    }
    return -EINVAL;
  }

  virt_addr_t base = page_align_down_addr((virt_addr_t)addr);
  size_t aligned_len = page_align_up_size(len);

  if(check_overflow(base, aligned_len)) {
    current->err_no = EINVAL;
    return -EINVAL;
  }

  vm_region_t* region = NULL;
  for(size_t i = 0; i < current->vm_region_count; i++) {
    vm_region_t* candidate = &current->vm_regions[i];
    if(candidate->base == base && candidate->type == VM_REGION_MMAP) {
      region = candidate;
      break;
    }
  }

  if(region == NULL || region->length != aligned_len) {
    current->err_no = EINVAL;
    return -EINVAL;
  }

  if(!vm_unmap(current, base, aligned_len)) {
    current->err_no = EFAULT;
    return -EFAULT;
  }

  if(base + aligned_len == current->mmap_next) {
    current->mmap_next = base;
  }

  current->err_no = 0;
  return 0;
}
