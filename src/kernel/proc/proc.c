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
#include <kernel/user/elf_loader.h>
#include <kernel/syscall.h>
#include <kernel/vm.h>
#include <errno.h>
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
  .priority = PROC_PRIO_IDLE,
  .stack_base = NULL,
  .state = PROC_STATE_READY
};

proc_info_p procs[PROC_MAX] = {
  &kernel_process_info
};

proc_info_p current = &kernel_process_info;

typedef struct scheduler_queue_t {
  proc_info_p head;
  proc_info_p tail;
} scheduler_queue_t;

static scheduler_queue_t ready_queues[PROC_PRIORITY_COUNT];
static proc_info_p sleep_queue_head = NULL;
static uint32_t scheduler_actions = 0;

#define SCHED_ACTION_FORCE (1u << 0)
#define SCHED_ACTION_SLEEP (1u << 1)

static const uint64_t scheduler_default_quanta[PROC_PRIORITY_COUNT] = {
  1000, 4000, 6000, 8000, 12000
};

static uint64_t scheduler_quantum_table[PROC_PRIORITY_COUNT] = {
  1000, 4000, 6000, 8000, 12000
};

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

static void scheduler_sleep_enqueue(proc_info_p proc);
static void scheduler_cleanup_process(proc_info_p proc);

static inline uint64_t scheduler_now_us(void) {
  return unix_time_us();
}

static void proc_release_user_memory(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  while(proc->vm_region_count > 0) {
    vm_region_t region = proc->vm_regions[proc->vm_region_count - 1];
    if(!vm_unmap(proc, region.base, region.length)) {
      serial_printf("proc_release_user_memory: failed to unmap region %lx length %zu\n",
                    region.base,
                    region.length);
      break;
    }
  }

  if(proc->vm_region_count != 0) {
    serial_printf("proc_release_user_memory: leaked %zu regions during teardown\n",
                  proc->vm_region_count);
  }
  proc->user_segment_count = 0;
}

static void proc_free_resources(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  proc_release_user_memory(proc);
  proc_file_table_cleanup(proc);

  if(proc->address_space_root) {
    pmm_free_pages(proc->address_space_root, 1);
    proc->address_space_root = 0;
  }

  if(proc->cpu_state) {
    kfree(proc->cpu_state);
    proc->cpu_state = NULL;
  }

  if(proc->stack_pointer) {
    kfree(proc->stack_pointer);
    proc->stack_pointer = NULL;
    proc->stack_base = NULL;
  }

  kfree(proc);
}

static void ready_queue_push(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  if(proc->priority > PROC_PRIORITY_MAX) {
    proc->priority = PROC_PRIO_NORMAL;
  }

  scheduler_queue_t* queue = &ready_queues[proc->priority];
  proc->next = NULL;
  if(queue->tail) {
    queue->tail->next = proc;
  } else {
    queue->head = proc;
  }
  queue->tail = proc;
}

static proc_info_p ready_queue_pop_at_priority(uint8_t priority) {
  if(priority > PROC_PRIORITY_MAX) {
    return NULL;
  }

  scheduler_queue_t* queue = &ready_queues[priority];
  while(queue->head) {
    proc_info_p proc = queue->head;
    queue->head = proc->next;
    if(queue->head == NULL) {
      queue->tail = NULL;
    }
    proc->next = NULL;

    if(proc->state == PROC_STATE_TERMINATED) {
      scheduler_cleanup_process(proc);
      continue;
    }

    if(proc->state == PROC_STATE_SLEEPING) {
      scheduler_sleep_enqueue(proc);
      continue;
    }

    return proc;
  }

  return NULL;
}

static proc_info_p ready_queue_pop_highest(void) {
  for(int pr = PROC_PRIORITY_MAX; pr >= PROC_PRIO_IDLE; --pr) {
    proc_info_p proc = ready_queue_pop_at_priority((uint8_t)pr);
    if(proc != NULL) {
      return proc;
    }
  }
  return NULL;
}

static uint8_t ready_queue_highest_priority(void) {
  for(int pr = PROC_PRIORITY_MAX; pr >= PROC_PRIO_IDLE; --pr) {
    if(ready_queues[pr].head != NULL) {
      return (uint8_t)pr;
    }
  }
  return PROC_PRIO_IDLE;
}

static void scheduler_sleep_enqueue(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  if(sleep_queue_head == NULL || proc->sleep_until < sleep_queue_head->sleep_until) {
    proc->next = sleep_queue_head;
    sleep_queue_head = proc;
    return;
  }

  proc_info_p node = sleep_queue_head;
  while(node->next && node->next->sleep_until <= proc->sleep_until) {
    node = node->next;
  }
  proc->next = node->next;
  node->next = proc;
}

static void scheduler_wake_sleepers(uint64_t now) {
  while(sleep_queue_head && sleep_queue_head->sleep_until <= now) {
    proc_info_p proc = sleep_queue_head;
    sleep_queue_head = proc->next;
    proc->next = NULL;
    proc->state = PROC_STATE_READY;
    proc->time_slice_remaining_us = proc->quantum_us;
    ready_queue_push(proc);
  }
}

static void scheduler_cleanup_process(proc_info_p proc) {
  if(proc == NULL || proc == &kernel_process_info) {
    return;
  }

  if(proc->stack_pointer) {
    kfree(proc->stack_pointer);
    proc->stack_pointer = NULL;
  }

  for(size_t seg = 0; seg < proc->user_segment_count; seg++) {
    if(proc->user_segments[seg].phys) {
      pmm_free_pages(proc->user_segments[seg].phys, proc->user_segments[seg].pages);
    }
  }
  proc->user_segment_count = 0;

  if(proc->address_space_root && proc->address_space_root != pmm_get_kernel_cr3()) {
    pmm_free_pages(proc->address_space_root, 1);
    proc->address_space_root = 0;
  }

  if(proc->cpu_state) {
    kfree(proc->cpu_state);
    proc->cpu_state = NULL;
  }

  kfree(proc);
}

void proc_debug(cpu_state_p state) {
  serial_printf("proc_debug: %p\n", state);
  serial_printf("            cs:  %lx ss:  %lx rfl: %lx\n", state->cs, state->ss, state->rflags);
  serial_printf("proc_debug: rbp: %lx rdi: %lx rip: %lx rsi: %lx\n", state->rbp, state->rdi, state->rip, state->rsi);
  serial_printf("proc_debug: rsp: %lx rax: %lx rbx: %lx rcx: %lx\n", state->rsp, state->rax, state->rbx, state->rcx);
  serial_printf("proc_debug: rdx: %lx r8:  %lx r9:  %lx r10: %lx\n", state->rdx, state->r8, state->r9, state->r10);
}

void proc_switch(void* arg) {
  if(current == NULL) {
    current = &kernel_process_info;
  }

  cpu_state_p frame = (cpu_state_p)arg;
  uint64_t now = scheduler_now_us();

  if(last_exec == 0) {
    last_exec = now;
  }

  uint64_t diff = now - last_exec;
  last_exec = now;

  if(current && current->cpu_state) {
    memcpy(current->cpu_state, frame, sizeof(cpu_state_t));
  }

  scheduler_wake_sleepers(now);

  bool forced = (scheduler_actions & SCHED_ACTION_FORCE) != 0;
  bool to_sleep = (scheduler_actions & SCHED_ACTION_SLEEP) != 0;
  scheduler_actions = 0;

  if(current != NULL && current->state == PROC_STATE_RUNNING) {
    current->exec_time += diff;
    if(diff < current->time_slice_remaining_us) {
      current->time_slice_remaining_us -= diff;
    } else {
      current->time_slice_remaining_us = 0;
    }
  }

  if(current != NULL && current->state == PROC_STATE_TERMINATED) {
    forced = true;
  }

  uint8_t highest_ready = ready_queue_highest_priority();
  if(!forced && current != NULL && current->state == PROC_STATE_RUNNING) {
    if(current->time_slice_remaining_us > 0 && highest_ready <= current->priority) {
      current->last_dispatch_us = now;
      return;
    }
  }

  proc_info_p previous = current;

  if(previous != NULL) {
    if(to_sleep) {
      previous->state = PROC_STATE_SLEEPING;
      scheduler_sleep_enqueue(previous);
    } else if(previous->state == PROC_STATE_TERMINATED) {
      scheduler_cleanup_process(previous);
      previous = NULL;
    } else if(previous->state == PROC_STATE_RUNNING) {
      if(previous != &kernel_process_info) {
        previous->state = PROC_STATE_READY;
        previous->time_slice_remaining_us = previous->quantum_us;
        ready_queue_push(previous);
      } else {
        previous->time_slice_remaining_us = previous->quantum_us;
      }
    }
  }

  proc_info_p next = ready_queue_pop_highest();
  if(next == NULL) {
    next = &kernel_process_info;
  }

  current = next;

  if(current->cpu_state == NULL) {
    current->cpu_state = kmalloc(sizeof(cpu_state_t));
    if(current->cpu_state == NULL) {
      halt();
    }
    memset(current->cpu_state, 0, sizeof(cpu_state_t));
  }

  current->state = PROC_STATE_RUNNING;
  current->dispatch_count++;
  current->time_slice_remaining_us = current->quantum_us;
  current->last_dispatch_us = now;

  phys_addr_t desired_cr3 = current->address_space_root ? current->address_space_root : pmm_get_kernel_cr3();
  if(read_cr3() != desired_cr3) {
    write_cr3(desired_cr3);
  }

  memcpy(frame, current->cpu_state, sizeof(cpu_state_t));
  if(current->user_mode) {
    frame->cs = USER_CODE_SEGMENT;
    frame->ss = USER_DATA_SEGMENT;
  } else {
    frame->cs = KERNEL_CODE_SEGMENT;
    frame->ss = KERNEL_DATA_SEGMENT;
  }

  uint64_t kernel_stack = proc_kernel_stack_top(current);
  if(kernel_stack != 0) {
    tss_update_kernel_stack(kernel_stack);
  }
}

void proc_create(proc_info_p proc, const char* name, void (*entrypoint)(void *), void* arg) {
  memset(proc->name, 0, sizeof(proc->name));
  strncpy(proc->name, name, sizeof(proc->name) - 1);
  proc->children = NULL;
  proc->children_count = 0;
  proc->parent = current;
  proc->pid = last_pid++;
  proc_file_table_init(proc);
  if(current != NULL) {
    proc_file_table_clone(proc, current);
  }

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
  proc->user_stack_base_vaddr = 0;
  proc->user_stack_size = 0;
  proc->mmap_base = 0;
  proc->mmap_next = 0;
  proc->mmap_limit = 0;
  proc->dispatch_count = 0;
  proc->exec_time = 0;
  proc->last_dispatch_us = 0;

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
  proc->address_space_root = pmm_get_kernel_cr3();
  proc->user_segment_count = 0;
  proc_set_priority(proc, PROC_PRIO_NORMAL);
  proc->time_slice_remaining_us = proc->quantum_us;
}

static inline virt_addr_t user_code_base(uint32_t pid) {
  const virt_addr_t base = 0x0000000000400000ull;
  const virt_addr_t stride = 0x200000ull; // 2MB per process
  return base + (stride * pid);
}

static inline virt_addr_t user_stack_top(uint32_t pid) {
  const virt_addr_t base = 0x0000000000800000ull;
  const virt_addr_t stride = 0x200000ull; // 2MB per process
  return base + (stride * pid);
}

static inline virt_addr_t user_mmap_base(uint32_t pid) {
  const virt_addr_t base = 0x0000000100000000ull;   // 4GB window per process slot
  const virt_addr_t stride = 0x0000000010000000ull; // 256MB per process
  return base + (stride * pid);
}

static inline virt_addr_t user_mmap_limit(uint32_t pid) {
  const virt_addr_t stride = 0x0000000010000000ull;
  return user_mmap_base(pid) + stride;
}

static bool proc_register_user_segment_internal(proc_info_p proc, phys_addr_t phys, size_t pages) {
  if(pages == 0 || phys == 0) {
    return false;
  }
  if(proc->user_segment_count >= PROC_MAX_USER_SEGMENTS) {
    return false;
  }
  proc->user_segments[proc->user_segment_count].phys = phys;
  proc->user_segments[proc->user_segment_count].pages = pages;
  proc->user_segment_count++;
  return true;
}

bool proc_register_user_segment(proc_info_p proc, phys_addr_t phys, size_t pages) {
  return proc_register_user_segment_internal(proc, phys, pages);
}

void proc_unregister_user_segment(proc_info_p proc, phys_addr_t phys, size_t pages) {
  if(proc == NULL || pages == 0) {
    return;
  }

  for(size_t i = 0; i < proc->user_segment_count; i++) {
    if(proc->user_segments[i].phys == phys && proc->user_segments[i].pages == pages) {
      for(size_t j = i + 1; j < proc->user_segment_count; j++) {
        proc->user_segments[j - 1] = proc->user_segments[j];
      }
      proc->user_segment_count--;
      break;
    }
  }
}

void scheduler_set_quantum(uint8_t priority, uint64_t quantum_us) {
  if(priority > PROC_PRIORITY_MAX || quantum_us == 0) {
    return;
  }
  scheduler_quantum_table[priority] = quantum_us;
}

uint64_t scheduler_get_quantum(uint8_t priority) {
  if(priority > PROC_PRIORITY_MAX) {
    priority = PROC_PRIO_NORMAL;
  }

  uint64_t quantum = scheduler_quantum_table[priority];
  if(quantum == 0) {
    quantum = scheduler_default_quanta[priority];
  }
  return quantum;
}

void proc_set_priority(proc_info_p proc, uint8_t priority) {
  if(proc == NULL) {
    return;
  }

  if(priority > PROC_PRIORITY_MAX) {
    priority = PROC_PRIO_NORMAL;
  }

  proc->priority = priority;
  proc->quantum_us = scheduler_get_quantum(priority);
  if(proc->quantum_us == 0) {
    proc->quantum_us = scheduler_default_quanta[PROC_PRIO_NORMAL];
  }
  proc->time_slice_remaining_us = proc->quantum_us;
}

void proc_request_yield(void) {
  scheduler_actions |= SCHED_ACTION_FORCE;
  if(current) {
    current->time_slice_remaining_us = 0;
  }
}

void proc_request_sleep(uint64_t duration_us) {
  if(current == NULL) {
    return;
  }

  if(duration_us == 0) {
    proc_request_yield();
    return;
  }

  current->sleep_until = scheduler_now_us() + duration_us;
  current->time_slice_remaining_us = 0;
  scheduler_actions |= (SCHED_ACTION_SLEEP | SCHED_ACTION_FORCE);
}

void proc_mark_ready(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  if(proc->state == PROC_STATE_TERMINATED || proc->state == PROC_STATE_READY) {
    return;
  }

  uint64_t flags;
  __asm__ volatile("pushfq; pop %0" : "=r"(flags) :: "memory");
  disable_interrupts();

  proc->state = PROC_STATE_READY;
  proc->time_slice_remaining_us = proc->quantum_us;
  proc->sleep_until = 0;
  ready_queue_push(proc);

  scheduler_actions |= SCHED_ACTION_FORCE;

  if(flags & (1ull << 9)) {
    enable_interrupts();
  }
}

proc_info_p proc_fork(proc_info_p parent, const syscall_frame_t* frame, int* err_out) {
  if(err_out) {
    *err_out = -ENOMEM;
  }

  if(parent == NULL || frame == NULL) {
    return NULL;
  }

  proc_info_p child = kmalloc(sizeof(proc_info_t));
  if(child == NULL) {
    return NULL;
  }
  memset(child, 0, sizeof(proc_info_t));
  proc_file_table_init(child);
  proc_file_table_clone(child, parent);

  child->parent = parent;
  child->pid = last_pid++;
  strncpy(child->name, parent->name, sizeof(child->name) - 1);
  child->name[sizeof(child->name) - 1] = '\0';
  child->user_mode = parent->user_mode;
  child->user_stack_base_vaddr = parent->user_stack_base_vaddr;
  child->user_stack_size = parent->user_stack_size;
  child->brk = parent->brk;
  child->heap = parent->heap;
  child->mmap_base = parent->mmap_base;
  child->mmap_next = parent->mmap_next;
  child->mmap_limit = parent->mmap_limit;
  child->errno = 0;
  child->exit_code = 0;
  child->sleep_until = 0;
  child->last_dispatch_us = 0;
  child->dispatch_count = 0;
  child->exec_time = 0;

  child->stack_pointer = kmalloc(PROC_STACK_SIZE + 0xF);
  if(child->stack_pointer == NULL) {
    proc_free_resources(child);
    return NULL;
  }
  uintptr_t aligned = ((uintptr_t)child->stack_pointer + 0xF) & ~((uintptr_t)0xF);
  child->stack_base = (uintptr_t*)aligned;
  memset(child->stack_base, 0, PROC_STACK_SIZE);

  child->cpu_state = kmalloc(sizeof(cpu_state_t));
  if(child->cpu_state == NULL) {
    proc_free_resources(child);
    return NULL;
  }
  memset(child->cpu_state, 0, sizeof(cpu_state_t));

  phys_addr_t new_root = pmm_clone_kernel_address_space();
  if(new_root == 0) {
    proc_free_resources(child);
    return NULL;
  }
  child->address_space_root = new_root;

  child->user_segment_count = 0;
  child->vm_region_count = 0;

  if(!vm_clone(child, parent)) {
    proc_free_resources(child);
    return NULL;
  }

  memcpy(child->cpu_state, frame, sizeof(syscall_frame_t));
  child->cpu_state->rax = 0;

  proc_set_priority(child, parent->priority);
  child->time_slice_remaining_us = child->quantum_us;
  child->state = PROC_STATE_READY;

  parent->children_count++;

  proc_execute(child);

  if(err_out) {
    *err_out = 0;
  }

  return child;
}

int proc_exec_image(proc_info_p proc, const uint8_t* image, size_t size, syscall_frame_t* frame) {
  if(proc == NULL || image == NULL || size == 0 || frame == NULL) {
    return -EINVAL;
  }

  uint8_t* elf_copy = kmalloc(size);
  if(elf_copy == NULL) {
    return -ENOMEM;
  }

  memcpy(elf_copy, image, size);

  phys_addr_t new_root = pmm_clone_kernel_address_space();
  if(new_root == 0) {
    kfree(elf_copy);
    return -ENOMEM;
  }

  proc_info_t staging;
  memset(&staging, 0, sizeof(staging));
  staging.pid = proc->pid;
  staging.address_space_root = new_root;

  proc_file_table_prepare_exec(proc);

  virt_addr_t stack_top = user_stack_top(proc->pid);
  virt_addr_t stack_base_vaddr = stack_top - PROC_USER_STACK_SIZE;
  staging.user_stack_base_vaddr = stack_base_vaddr;
  staging.user_stack_size = PROC_USER_STACK_SIZE;

  int result = -ENOMEM;

  if(!vm_region_add(&staging,
                    stack_base_vaddr,
                    PROC_USER_STACK_SIZE,
                    VM_REGION_STACK,
                    VM_REGION_FLAG_READ | VM_REGION_FLAG_WRITE | VM_REGION_FLAG_USER | VM_REGION_FLAG_GROW_DOWN)) {
    goto fail;
  }

  vm_region_t* stack_region = vm_region_find(&staging, stack_base_vaddr);
  if(stack_region == NULL) {
    goto fail;
  }

  phys_addr_t initial_stack_phys = pmm_alloc_pages(1);
  if(initial_stack_phys == 0) {
    goto fail;
  }

  void* stack_page_ptr = (void*)physical_to_virtual(initial_stack_phys);
  memset(stack_page_ptr, 0, PAGE_SIZE);

  virt_addr_t initial_stack_page = stack_top - PAGE_SIZE;
  if(!pmm_map_page_in_root(new_root, initial_stack_page, initial_stack_phys, true, true)) {
    pmm_free_pages(initial_stack_phys, 1);
    goto fail;
  }

  if(!proc_register_user_segment(&staging, initial_stack_phys, 1)) {
    pmm_unmap_page_in_root(new_root, initial_stack_page);
    pmm_free_pages(initial_stack_phys, 1);
    goto fail;
  }

  vm_region_note_mapping(stack_region, initial_stack_page, PAGE_SIZE);

  uint64_t entry = 0;
  if(!elf64_load_image(&staging, new_root, elf_copy, size, &entry)) {
    result = -ENOEXEC;
    goto fail;
  }

  proc->brk = 0;
  proc->heap = 0;

  phys_addr_t old_root = proc->address_space_root;
  proc_release_user_memory(proc);

  if(old_root != 0 && old_root != new_root && old_root != pmm_get_kernel_cr3()) {
    pmm_free_pages(old_root, 1);
  }

  proc->address_space_root = new_root;
  proc->user_stack_base_vaddr = staging.user_stack_base_vaddr;
  proc->user_stack_size = staging.user_stack_size;
  proc->user_segment_count = staging.user_segment_count;
  memcpy(proc->user_segments,
         staging.user_segments,
         staging.user_segment_count * sizeof(proc_user_segment_t));
  proc->vm_region_count = staging.vm_region_count;
  memcpy(proc->vm_regions,
         staging.vm_regions,
         staging.vm_region_count * sizeof(vm_region_t));
  proc->user_mode = true;
  proc->mmap_base = user_mmap_base(proc->pid);
  proc->mmap_next = proc->mmap_base;
  proc->mmap_limit = user_mmap_limit(proc->pid);

  kfree(elf_copy);

  if(current == proc) {
    write_cr3(proc->address_space_root);
  }

  frame->rip = entry;
  frame->rsp = stack_top;
  frame->rbp = stack_top;
  frame->rax = 0;
  frame->rbx = 0;
  frame->rcx = 0;
  frame->rdx = 0;
  frame->rsi = 0;
  frame->rdi = 0;
  frame->r8 = 0;
  frame->r9 = 0;
  frame->r10 = 0;
  frame->r11 = 0;
  frame->r12 = 0;
  frame->r13 = 0;
  frame->r14 = 0;
  frame->r15 = 0;
  frame->cs = USER_CODE_SEGMENT;
  frame->ss = USER_DATA_SEGMENT;
  frame->rflags = 0x202;

  if(proc->cpu_state == NULL) {
    proc->cpu_state = kmalloc(sizeof(cpu_state_t));
    if(proc->cpu_state == NULL) {
      return -ENOMEM;
    }
  }

  memcpy(proc->cpu_state, frame, sizeof(syscall_frame_t));

  return 0;

fail:
  proc_release_user_memory(&staging);
  if(new_root != 0) {
    pmm_free_pages(new_root, 1);
  }
  kfree(elf_copy);
  return result;
}

bool proc_user_buffer_accessible(proc_info_p proc, const void* ptr, size_t length) {
  if(proc == NULL || ptr == NULL) {
    return false;
  }

  if(length == 0) {
    return true;
  }

  uintptr_t start = (uintptr_t)ptr;
  uintptr_t last = start;
  if(__builtin_add_overflow(start, length - 1, &last)) {
    return false;
  }

  for(size_t i = 0; i < proc->vm_region_count; i++) {
    vm_region_t* region = &proc->vm_regions[i];
    if((region->flags & VM_REGION_FLAG_USER) == 0) {
      continue;
    }

    virt_addr_t committed_start = region->committed_base;
    virt_addr_t committed_end = region->committed_top;

    if(committed_start >= committed_end) {
      continue;
    }

    if(start >= committed_start && last < committed_end) {
      return true;
    }
  }

  return false;
}

void proc_create_user(proc_info_p proc, const char* name, const void* code_blob, size_t code_size, void* arg) {
  proc_create(proc, name, NULL, arg);

  proc->user_mode = true;

  if(code_size == 0) {
    serial_printf("proc_create_user: code blob is empty for process %s\n", name);
    halt();
  }

  phys_addr_t new_root = pmm_clone_kernel_address_space();
  if(new_root == 0) {
    serial_printf("proc_create_user: failed to clone kernel address space\n");
    halt();
  }

  virt_addr_t stack_top = user_stack_top(proc->pid);
  virt_addr_t stack_base_vaddr = stack_top - PROC_USER_STACK_SIZE;

  proc->user_stack_base_vaddr = stack_base_vaddr;
  proc->user_stack_size = PROC_USER_STACK_SIZE;
  proc->address_space_root = new_root;
  proc->mmap_base = user_mmap_base(proc->pid);
  proc->mmap_next = proc->mmap_base;
  proc->mmap_limit = user_mmap_limit(proc->pid);

  if(!vm_region_add(proc,
                    stack_base_vaddr,
                    PROC_USER_STACK_SIZE,
                    VM_REGION_STACK,
                    VM_REGION_FLAG_READ | VM_REGION_FLAG_WRITE | VM_REGION_FLAG_USER | VM_REGION_FLAG_GROW_DOWN)) {
    serial_printf("proc_create_user: failed to register stack region\n");
    halt();
  }

  // Map the initial stack page at the top of the region so early frames work without faults
  phys_addr_t initial_stack_phys = pmm_alloc_pages(1);
  if(initial_stack_phys == 0) {
    serial_printf("proc_create_user: failed to allocate initial stack page\n");
    halt();
  }

  void* initial_page_ptr = (void*)physical_to_virtual(initial_stack_phys);
  memset(initial_page_ptr, 0, PAGE_SIZE);

  virt_addr_t initial_stack_page = stack_top - PAGE_SIZE;
  if(!pmm_map_page_in_root(new_root, initial_stack_page, initial_stack_phys, true, true)) {
    serial_printf("proc_create_user: failed to map initial stack page\n");
    pmm_free_pages(initial_stack_phys, 1);
    halt();
  }

  if(!proc_register_user_segment(proc, initial_stack_phys, 1)) {
    serial_printf("proc_create_user: failed to register stack segment phys=%lx\n",
                  initial_stack_phys);
    pmm_free_pages(initial_stack_phys, 1);
    halt();
  }

  vm_region_t* stack_region = vm_region_find(proc, initial_stack_page);
  if(stack_region != NULL) {
    vm_region_note_mapping(stack_region, initial_stack_page, PAGE_SIZE);
  }

  uint64_t entry = 0;
  if(!elf64_load_image(proc, new_root, code_blob, code_size, &entry)) {
    serial_printf("proc_create_user: ELF loading failed for process %s\n", name);
    halt();
  }

  proc->cpu_state->rip = entry;
  proc->cpu_state->rdi = (uint64_t)arg;
  proc->cpu_state->rsp = stack_top;
  proc->cpu_state->rbp = stack_top;
  proc->cpu_state->cs = USER_CODE_SEGMENT;
  proc->cpu_state->ss = USER_DATA_SEGMENT;
  proc->cpu_state->rflags = 0x202;
}

void proc_execute(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  proc->state = PROC_STATE_READY;
  proc->time_slice_remaining_us = proc->quantum_us;
  proc->last_dispatch_us = 0;
  ready_queue_push(proc);
  serial_printf("proc_execute: queued process %s (priority %u)\n", proc->name, proc->priority);
}

void scheduler_init() {
  logk("Initing scheduler");
  current = &kernel_process_info;
  memset(ready_queues, 0, sizeof(ready_queues));
  sleep_queue_head = NULL;
  scheduler_actions = 0;

  for(uint8_t pr = 0; pr < PROC_PRIORITY_COUNT; pr++) {
    if(scheduler_quantum_table[pr] == 0) {
      scheduler_quantum_table[pr] = scheduler_default_quanta[pr];
    }
  }

  if(current->cpu_state == NULL) {
    current->cpu_state = (cpu_state_t*)kmalloc(sizeof(cpu_state_t));
    if(current->cpu_state == NULL) {
      halt();
    }
  }
  memset(current->cpu_state, 0, sizeof(cpu_state_t));

  current->pid = last_pid++;
  current->address_space_root = pmm_get_kernel_cr3();
  proc_set_priority(current, PROC_PRIO_IDLE);
  current->state = PROC_STATE_RUNNING;
  current->dispatch_count = 1;
  current->last_dispatch_us = scheduler_now_us();

  uint64_t kernel_stack = proc_kernel_stack_top(current);
  if(kernel_stack != 0) {
    tss_update_kernel_stack(kernel_stack);
  }
  register_timer_callback(proc_switch);
  printf(".OK\n");
}

void proc_exit(int code) {
  current->state = PROC_STATE_TERMINATED;
  current->exit_code = code;
  serial_printf("proc_exit: Process %s exited with code %d\n", current->name, code);
  scheduler_actions |= SCHED_ACTION_FORCE;
}
