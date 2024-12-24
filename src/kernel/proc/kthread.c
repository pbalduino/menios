#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/thread.h>
#include <kernel/tsc.h>

#include <errno.h>
#include <string.h>

void kthread_execute(void* arg) {
  kthread_t* thread = (kthread_t*)arg;
  serial_printf("thread_execute: argument for %s is null? %s\n", thread->name, thread->arguments == NULL ? "YES" : "NO");
  serial_line("");
  int res = (thread->entrypoint)(thread->arguments);
  serial_line("");
  kexit(res);
  serial_line("");
  while(true) {
  }
  serial_line("");
}

int kthread_create(kthread_t* thread, const char* name, int (*entrypoint)(void *), void* arg) {
  thread->name = name;
  thread->entrypoint = entrypoint;
  thread->arguments = arg;
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
  uint64_t end = start + (milliseconds * 1000000);
  current->sleep_until = end;
  current->state = THREAD_SLEEPING;
  while(read_tsc() < end) {
    noop();
  }
}

void kexit(int code) {
  proc_exit(code);
  serial_printf("kexit: process '%s' is terminated with code '%d' and waiting to be finished\n", current->name, current->exit_code);
  while(true) {
    noop();
   }
}

void ktread_join(kthread_t* t1) {
  while(t1->status != THREAD_TERMINATED) {
    noop();
  }
}