#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/mman.h>
#include <types.h>
#include <unistd.h>

void init_brk() {
  serial_printf("init_brk: current->heap: %lx\n", current->heap);

  uintptr_t value = get_first_free_page();
  if(!value) {
    serial_printf("init_brk: no free page\n");
    return;
  }

  current->heap = physical_to_virtual(value);
  
  current->brk = current->heap;

  serial_printf("init_brk: free page: %lx\n", get_first_free_page());
  serial_printf("init_brk: heap_offset: %lx\n", current->heap);
}

void* kmmap_anonymous(void *addr, size_t length, int prot) {
  serial_printf("mmap_anonymous: addr: %lx, length: %lx, prot: %d\n", addr, length, prot);
  serial_printf("mmap_anonymous: current->brk: %lx\n", current->brk);

  if(current->brk == 0) {
    init_brk();
    if(current->brk == 0) {
      return MAP_FAILED;
    }
  }

  if(addr == NULL) {
    uintptr_t value = get_first_free_virtual_address(physical_to_virtual(current->brk));
    if(!value) {
      return MAP_FAILED;
    }
    addr = (void*)value;
  }

  if(length == 0) {
    length = PAGE_SIZE;
  }

  if(length % PAGE_SIZE != 0) {
    length = ((length / PAGE_SIZE) + 1) * PAGE_SIZE;
  }

  void* brk = sbrk(length);

  if(brk != MAP_FAILED) {
    set_page_used(virtual_to_physical(current->brk));
  }

  return brk;
}

void* kmmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  switch (flags) {
  case MAP_ANONYMOUS:
  case MAP_ANONYMOUS | MAP_PRIVATE:
    return kmmap_anonymous(addr, length, prot);
    break;
  
  default:
    serial_printf("mmap: unknown flag: %d\n", flags);
    return NULL;
    break;
  }
}

int kmunmap(void *addr, size_t len) {
  serial_printf("munmap: addr: %lx, len: %lx\n", addr, len);
  return 0; 
}