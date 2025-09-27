#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/thread.h>
#include <kernel/tsc.h>
#include <kernel/atomic.h>

#include <errno.h>
#include <string.h>

void kthread_execute(void* arg) {
  kthread_t* thread = (kthread_t*)arg;
  serial_printf("thread_execute: argument for %s is null? %s\n", thread->name, thread->arguments == NULL ? "YES" : "NO");
  serial_line("");
  int res = 0;
  if(thread->entrypoint != NULL) {
    res = (thread->entrypoint)(thread->arguments);
  } else {
    serial_error("kthread_execute: entrypoint is NULL\n");
    res = -EINVAL;
  }
  atomic_store32(&thread->exit_code, res, memory_order_release);
  atomic_store32(&thread->status, THREAD_TERMINATED, memory_order_release);
  serial_line("");
  kexit(res);
}

int kthread_create(kthread_t* thread, const char* name, int (*entrypoint)(void *), void* arg) {
  thread->name = name;
  thread->entrypoint = entrypoint;
  thread->arguments = arg;
  atomic_store32(&thread->status, THREAD_RUNNING, memory_order_relaxed);
  atomic_store32(&thread->exit_code, 0, memory_order_relaxed);
  serial_printf("kthread_create: argument for %s is null? %s\n", thread->name, thread->arguments == NULL ? "YES" : "NO");

  serial_line("");
  void* foo = kmalloc(sizeof(proc_info_t));
  serial_line("");
  if(current->errno == ENOMEM) {
    serial_error("kthread_create: Out of memory\n");
    halt();
  }
  serial_line("");
  proc_info_p proc = (proc_info_p)foo;
  memzero(proc, sizeof(proc_info_t));
  serial_line("");

  serial_printf("kthread_create: proc %s @ %p\n", thread->name, proc);
  proc_create(proc, thread->name, kthread_execute, thread);
  serial_printf("kthread_create: proc @ %p after create\n", proc);
  proc_execute(proc);
  serial_printf("kthread_create: proc @ %p after execute\n", proc);
  return 0;
}

void ksleep(uint64_t milliseconds) {
  uint64_t start = read_tsc();
  uint64_t duration_ticks = tsc_ns_to_ticks(milliseconds * 1000000ull);
  uint64_t end = start + duration_ticks;
  current->sleep_until = end;
  current->state = PROC_STATE_SLEEPING;
  while(read_tsc() < end) {
    noop();
  }
  current->sleep_until = 0;
  current->state = PROC_STATE_RUNNING;
}

void kexit(int code) {
  proc_exit(code);
  serial_printf("kexit: process '%s' terminated with code %d\n", current->name, current->exit_code);
  enable_interrupts();
  for(;;) {
    asm volatile("hlt");
  }
}

int ktread_join(kthread_t* thread) {
  while(atomic_load32(&thread->status, memory_order_acquire) != THREAD_TERMINATED) {
    asm volatile("pause");
  }
  return (int)atomic_load32(&thread->exit_code, memory_order_acquire);
}
