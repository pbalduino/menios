#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <sys/mman.h>
#include <types.h>
#include <unistd.h>

void init_brk() {
  current->heap = physical_to_virtual(get_first_free_page());
  current->brk = current->heap;

  serial_printf("init_brk: heap_offset: %lx\n", current->heap);
}

void* mmap_anonymous(void *addr, size_t length, int prot) {
  uintptr_t vaddr = (uintptr_t)addr;

  if(current->brk == 0) {
    init_brk();
  }

  if(addr == NULL) {
    vaddr = get_first_free_virtual_address(physical_to_virtual(current->brk));
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

void* mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  uint64_t vaddr = (uint64_t)addr;

  switch (flags) {
  case MAP_ANONYMOUS:
  case MAP_ANONYMOUS | MAP_PRIVATE:
    return mmap_anonymous(addr, length, prot);
    break;
  
  default:
    serial_printf("mmap: unknown flag: %d\n", flags);
    return NULL;
    break;
  }
}