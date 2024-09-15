#include <kernel/proc.h>
#include <kernel/serial.h>
#include <types.h>
#include <sys/mman.h>

void *sbrk(intptr_t increment) {
  if(current->heap == 0 && current->brk == 0) {
    serial_printf("sbrk: ERROR: Heap was not initialized.\n");
    return MAP_FAILED;
  }

  serial_printf("sbrk: increment: %lx\n", increment);
  serial_printf("sbrk: curr brk: %lx\n", current->brk);

  uintptr_t new_brk = current->brk + increment;
  uintptr_t old_brk = current->brk;

  if(increment > RLIMIT_DATA ||
    new_brk < current->heap) {
    return MAP_FAILED;
  }

  current->brk = new_brk;

  serial_printf("sbrk: new brk: %lx\n", new_brk);

  return (void*)old_brk;
}