#include <kernel/proc.h>
#include <kernel/gdt.h>
#include <kernel/heap.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <kernel/timer.h>
#include <stdio.h>
#include <string.h>
#include <types.h>

uint64_t last_pid = 0;

proc_info_t kernel_process_info = {
  .children = NULL,
  .children_count = 0,
  .name = "kernel",
  .parent = NULL,
  .priority = PROC_PRIO_NORMAL,
  .stack = NULL,
  .state = PROC_STATE_READY
};

proc_info_p procs[PROC_MAX] = {
  &kernel_process_info
};

proc_info_p current = &kernel_process_info;

void proc_switch(void* arg) {
  cpu_state_p state = (cpu_state_p)arg;
  // serial_printf("states: rax: %lx - rbx: %lx - rcx: %lx - rdx: %lx - rsi: %lx\n", state->rax, state->rbx, state->rcx, state->rdx, state->rsi);
  // serial_printf("states: rdi: %lx - rsp: %lx - r8:  %lx - r9:  %lx - r10: %lx\n", state->rdi, state->rsp, state->r8, state->r9, state->r10);
  // serial_printf("states: r11: %lx\n", state->r11);
  // serial_printf("states: rip: %lx\n", state->rip);
  // serial_printf("states: cs:  %lx\n", state->cs);
  // serial_printf("states: rfl: %lx\n", state->rflags);
  // serial_printf("states: rsp: %lx\n", state->rsp);
  // serial_printf("states: cr3: %lx\n", state->cr3);
  // serial_printf("states: rbp: %lx\n", state->rbp);
  // serial_printf("states: ss:  %lx\n", state->ss);
  serial_line("");
  
  // serial_printf("proc_switch: Current process %s\n", current->name);
  // memcpy(current->cpu_state, state, sizeof(cpu_state_t));
/*
  current->state = PROC_STATE_READY;
  serial_line("");
  current = current->next;
  serial_line("");
  if(current->state == PROC_STATE_NEW) {
    serial_line("");
    current->state = PROC_STATE_READY;
    serial_line("");
    current->cpu_state->rip = (uint64_t)current->entrypoint;
    serial_line("");
    current->cpu_state->rdi = (uint64_t)current->arguments;
    serial_line("");
    current->cpu_state->rsp = (uint64_t)current->stack + PROC_STACK_SIZE;
    serial_line("");
    current->cpu_state->rbx = 0;
    serial_line("");
    current->cpu_state->rbp = current->cpu_state->rsp;
    serial_line("");
    current->cpu_state->rsi = 0;
    serial_line("");
    current->cpu_state->rflags = 0x02;
    serial_line("");
    current->cpu_state->cr3 = read_cr3();
    serial_line("");
    // (current->entrypoint)(current->arguments);
    // serial_line("");
  }
  serial_line("");
  serial_printf("proc_switch: Switching to process %s\n", current->name);
  */
}

void proc_create(proc_info_p proc, void (*entrypoint)(void *), void* arg) {
  proc->children = NULL;
  proc->children_count = 0;
  proc->cpu_state = (cpu_state_t*)kmalloc(sizeof(cpu_state_t));
  proc->parent = current;
  proc->pid = last_pid++;
  proc->priority = PROC_PRIO_NORMAL;
  proc->stack = kmalloc(PROC_STACK_SIZE);
  proc->state = PROC_STATE_NEW;
  proc->entrypoint = entrypoint;
  proc->arguments = arg;
}

void proc_execute(proc_info_p proc) {
  proc->next = current->next;
  current->next = proc;

  serial_printf("proc_execute: Executing process %s\n", proc->name);
}

void init_scheduler() {
  printf("- Initing scheduller");
  current = &kernel_process_info;
  current->next = current;
  current->cpu_state = (cpu_state_t*)kmalloc(sizeof(cpu_state_t));
  current->pid = last_pid++;
  register_timer_callback(proc_switch);
  printf(".OK\n");
}
