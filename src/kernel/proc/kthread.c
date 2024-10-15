#include <kernel/gdt.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/serial.h>

#include <string.h>

int kthread_create(kthread_t* thread, void (*entrypoint)(void *), void* arg) {
  proc_info_p proc = kmalloc(sizeof(proc_info_p));
  serial_line("");
  proc_create(proc, thread->name, entrypoint, arg);
  serial_line("");
  proc->cpu_state->cs = KERNEL_CODE_SEGMENT;
  proc->cpu_state->ss = KERNEL_DATA_SEGMENT;
  serial_line("");
  proc_execute(proc);
  serial_line("");

  return 0;
}