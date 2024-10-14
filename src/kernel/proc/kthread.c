#include <kernel/gdt.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/thread.h>

#include <string.h>

int kthread_create(kthread_t* thread, void (*entrypoint)(void *), void* arg) {
  proc_info_p proc = kmalloc(sizeof(proc_info_p));
  
  strncpy(proc->name, "kthread\0", 8);
  proc_create(proc, entrypoint, arg);
  proc->cpu_state->cs = KERNEL_CODE_SEGMENT;
  proc->cpu_state->ss = KERNEL_DATA_SEGMENT;
  proc_execute(proc);

  return 0;
}