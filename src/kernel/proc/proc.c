#include <kernel/console.h>
#include <kernel/gdt.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/thread.h>
#include <kernel/tsc.h>
#include <kernel/timer.h>
#include <stdio.h>
#include <string.h>
#include <types.h>

uint64_t last_pid = 0;
static uint64_t last_exec = 0;

proc_info_t kernel_process_info = {
  .children = NULL,
  .children_count = 0,
  .name = "kernel",
  .parent = NULL,
  .priority = PROC_PRIO_NORMAL,
  .stack_base = NULL,
  .state = PROC_STATE_READY
};

proc_info_p procs[PROC_MAX] = {
  &kernel_process_info
};

proc_info_p current = &kernel_process_info;

static inline uint64_t proc_kernel_stack_top(proc_info_p proc) {
  if(proc == NULL) {
    return 0;
  }

  if(proc->stack_base == NULL) {
    uint64_t rsp;
    asm volatile("mov %%rsp, %0" : "=r" (rsp));
    return rsp;
  }

  return (uint64_t)((uintptr_t)proc->stack_base + PROC_STACK_SIZE);
}

static char* proc_state(proc_state_t state) {
  switch(state)
  {
  case PROC_STATE_NEW:
    return "NEW";
    break;
  case PROC_STATE_READY:
    return "READY";
    break;
  case PROC_STATE_RUNNING:
    return "RUNNING";
    break;
  case PROC_STATE_WAITING:
    return "WAITING";
    break;
  case PROC_STATE_SLEEPING:
    return "SLEEPING";
    break;
  case PROC_STATE_TERMINATED:
    return "TERMINATED";
    break;
  default:
    serial_printf("proc_state: Unknown state %d\n", state);
    return "UNKNOWN";
    break;
  }
}

void proc_debug(cpu_state_p state) {
  serial_printf("proc_debug: %p\n", state);
  serial_printf("            cs:  %lx ss:  %lx rfl: %lx\n", state->cs, state->ss, state->rflags);
  serial_printf("proc_debug: rbp: %lx rdi: %lx rip: %lx rsi: %lx\n", state->rbp, state->rdi, state->rip, state->rsi);
  serial_printf("proc_debug: rsp: %lx rax: %lx rbx: %lx rcx: %lx\n", state->rsp, state->rax, state->rbx, state->rcx);
  serial_printf("proc_debug: rdx: %lx r8:  %lx r9:  %lx r10: %lx\n", state->rdx, state->r8, state->r9, state->r10);
}

void proc_switch(void* arg) {
  if(last_exec == 0) {
    last_exec = unix_time_us();
  } else {
    uint64_t now = unix_time_us();
    uint64_t diff = now - last_exec;
    last_exec = now;
    current->exec_time += diff;
    serial_printf("proc_switch: Process %s executed for %lu.%lu\n", current->name, diff / 1000000000, diff % 1000000000);
  }

  if(current->pid == current->next->pid) {
    serial_printf("Next process is the same. Skipping switch.\n");
    return;
  }

  cpu_state_p state = (cpu_state_p)arg;
  // proc_debug(state);
  memcpy(current->cpu_state, state, sizeof(cpu_state_t));

  serial_puts("proc_switch: Entering with ");
  proc_info_p node = current;

  for(int i = 0; i < 5; i++) {
    serial_printf("%s(%s) -> ", node->name, proc_state(node->state));
    node = node->next;
  }
  serial_puts("\n");

  proc_info_p next = current->next;

  while(next->state == PROC_STATE_TERMINATED) {
    current->next = next->next;
    serial_printf("proc_switch: Removing terminated process %s\n", next->name);

    // Free resources of the terminated process
    kfree(next->stack_pointer);
    if(next->user_stack_pointer) {
      kfree(next->user_stack_pointer);
    }
    kfree(next->cpu_state);
    kfree(next);

    next = current->next; // Move to the next process
  }

  if(current->state != PROC_STATE_TERMINATED) {
    current->state = PROC_STATE_READY;
  }
  
  current = current->next;
  memcpy(arg, current->cpu_state, sizeof(cpu_state_t));
  current->state = PROC_STATE_RUNNING;

  uint64_t kernel_stack = proc_kernel_stack_top(current);
  if(kernel_stack != 0) {
    tss_update_kernel_stack(kernel_stack);
  }

  serial_printf("proc_switch: Switching to process %s\n", current->name);

  serial_puts("proc_switch: Exiting with ");
  node = current;
  for(int i = 0; i < 5; i++) {
    serial_printf("%s(%s) -> ", node->name, proc_state(node->state));
    node = node->next;
  }
  serial_puts("\n");
}

void proc_create(proc_info_p proc, const char* name, void (*entrypoint)(void *), void* arg) {
  memset(proc->name, 0, sizeof(proc->name));
  strncpy(proc->name, name, sizeof(proc->name) - 1);
  proc->children = NULL;
  proc->children_count = 0;
  proc->parent = current;
  proc->pid = last_pid++;
  proc->priority = PROC_PRIO_NORMAL;

  serial_printf("proc_create: Creating process %s - %s with arg %lx\n", name, proc->name, arg);

  // ensure the stack is aligned to a 16-byte boundary
  proc->stack_pointer = kmalloc(PROC_STACK_SIZE + 0xF);
  if(proc->stack_pointer == NULL) {
    serial_printf("proc_create: Failed to allocate stack for process %s\n", name);
    halt();
  }
  uintptr_t aligned = ((uintptr_t)proc->stack_pointer + 0xF) & ~((uintptr_t)0xF);
  proc->stack_base = (uintptr_t*)aligned;
  memset(proc->stack_base, 0, PROC_STACK_SIZE);
  serial_printf("proc_create: entrypoint @ %p, args @ %p\n", entrypoint, arg);
  proc->state = PROC_STATE_NEW;
  proc->entrypoint = entrypoint;
  proc->arguments = arg;
  proc->user_mode = false;
  proc->user_stack_pointer = NULL;
  proc->user_stack_base = NULL;
  proc->user_stack_size = 0;

  proc->cpu_state = kmalloc(sizeof(cpu_state_t));
  if(proc->cpu_state == NULL) {
    serial_printf("proc_create: Failed to allocate cpu state for process %s\n", name);
    halt();
  }
  memset(proc->cpu_state, 0, sizeof(cpu_state_t));
  proc->cpu_state->rip = (uint64_t)entrypoint;
  proc->cpu_state->rdi = (uint64_t)arg;
  proc->cpu_state->rsp = (uint64_t)proc->stack_base + PROC_STACK_SIZE;
  proc->cpu_state->rbp = proc->cpu_state->rsp;
  proc->cpu_state->rflags = 0x246;
  serial_printf("proc_create: cs: %lx ss: %lx\n", current->cpu_state->cs, current->cpu_state->ss);
  proc->cpu_state->cs = KERNEL_CODE_SEGMENT;
  proc->cpu_state->ss = KERNEL_DATA_SEGMENT;
}

void proc_create_user(proc_info_p proc, const char* name, void (*entrypoint)(void *), void* arg) {
  proc_create(proc, name, entrypoint, arg);

  proc->user_mode = true;
  proc->user_stack_pointer = kmalloc(PROC_USER_STACK_SIZE + 0xF);
  if(proc->user_stack_pointer == NULL) {
    serial_printf("proc_create_user: Failed to allocate user stack for process %s\n", name);
    halt();
  }

  uintptr_t aligned = ((uintptr_t)proc->user_stack_pointer + 0xF) & ~((uintptr_t)0xF);
  proc->user_stack_base = (uintptr_t*)aligned;
  proc->user_stack_size = PROC_USER_STACK_SIZE;
  memset(proc->user_stack_base, 0, PROC_USER_STACK_SIZE);

  proc->cpu_state->rip = (uint64_t)entrypoint;
  proc->cpu_state->rdi = (uint64_t)arg;
  proc->cpu_state->rsp = (uint64_t)proc->user_stack_base + PROC_USER_STACK_SIZE;
  proc->cpu_state->rbp = proc->cpu_state->rsp;
  proc->cpu_state->cs = USER_CODE_SEGMENT;
  proc->cpu_state->ss = USER_DATA_SEGMENT;
  proc->cpu_state->rflags = 0x202;

  if(!pmm_mark_range_user((virt_addr_t)proc->user_stack_base, PROC_USER_STACK_SIZE)) {
    serial_printf("proc_create_user: failed to mark user stack as user-accessible\n");
    halt();
  }

  if(!pmm_mark_range_user((virt_addr_t)entrypoint, PAGE_SIZE)) {
    serial_printf("proc_create_user: failed to mark entrypoint page as user-accessible\n");
    halt();
  }
}

void proc_execute(proc_info_p proc) {
  serial_line("");
  proc->next = current->next;
  serial_line("");
  current->next = proc;
  serial_printf("proc_execute: Executing process %s\n", proc->name);
}

void scheduler_init() {
  logk("Initing scheduler");
  current = &kernel_process_info;
  current->next = current;
  current->cpu_state = (cpu_state_t*)kmalloc(sizeof(cpu_state_t));
  current->pid = last_pid++;
  uint64_t kernel_stack = proc_kernel_stack_top(current);
  if(kernel_stack != 0) {
    tss_update_kernel_stack(kernel_stack);
  }
  register_timer_callback(proc_switch);
  printf(".OK\n");
}

void proc_exit(int code) {
  serial_line("");
  current->state = PROC_STATE_TERMINATED;
  serial_line("");
  current->exit_code = code;
  serial_printf("proc_exit: Process %s exited with code %d\n", current->name, code);
}
