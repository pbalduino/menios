#include <kernel/console.h>
#include <kernel/gdt.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/context_switch.h>
#include <kernel/serial.h>
#include <kernel/syscall_entry.h>
#include <kernel/thread.h>
#include <kernel/tsc.h>
#include <kernel/timer.h>
#include <kernel/shm.h>
#include <kernel/user/elf_loader.h>
#include <kernel/syscall.h>
#include <kernel/vm.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <types.h>

uint64_t last_pid = 0;
static uint64_t last_exec = 0;

proc_info_t kernel_process_info = {
  .first_child = NULL,
  .sibling_next = NULL,
  .children_count = 0,
  .name = "kernel",
  .parent = NULL,
  .priority = PROC_PRIO_IDLE,
  .stack_base = NULL,
  .state = PROC_STATE_READY,
  .cwd = "/",
  .cwd_len = 1,
  .stop_status = 0,
  .continue_status = 0,
  .stopped = false,
  .stop_status_pending = false,
  .continued_pending = false
};

proc_info_p procs[PROC_MAX] = {
  &kernel_process_info
};

proc_info_p current = &kernel_process_info;

proc_info_p proc_find_by_pid(uint32_t pid) {
  for(size_t i = 0; i < PROC_MAX; i++) {
    proc_info_p proc = procs[i];
    if(proc != NULL && proc->pid == pid) {
      return proc;
    }
  }
  return NULL;
}

typedef struct scheduler_queue_t {
  proc_info_p head;
  proc_info_p tail;
} scheduler_queue_t;

static scheduler_queue_t ready_queues[PROC_PRIORITY_COUNT];
static proc_info_p sleep_queue_head = NULL;
static uint32_t scheduler_actions = 0;

static inline int encode_stopped_status(int signo) {
  return ((signo & 0x7f) << 8) | 0x7f;
}

static void ready_queue_remove(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  for(int pr = PROC_PRIO_IDLE; pr <= PROC_PRIORITY_MAX; ++pr) {
    scheduler_queue_t* queue = &ready_queues[pr];
    proc_info_p prev = NULL;
    proc_info_p node = queue->head;
    while(node) {
      if(node == proc) {
        if(prev) {
          prev->next = node->next;
        } else {
          queue->head = node->next;
        }
        if(queue->tail == node) {
          queue->tail = prev;
        }
        node->next = NULL;
        return;
      }
      prev = node;
      node = node->next;
    }
  }
}

static void sleep_queue_remove(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  proc_info_p prev = NULL;
  proc_info_p node = sleep_queue_head;
  while(node) {
    if(node == proc) {
      if(prev) {
        prev->next = node->next;
      } else {
        sleep_queue_head = node->next;
      }
      node->next = NULL;
      return;
    }
    prev = node;
    node = node->next;
  }
}

static void proc_link_child(proc_info_p parent, proc_info_p child) {
  if(parent == NULL || child == NULL) {
    return;
  }
  child->sibling_next = parent->first_child;
  parent->first_child = child;
  parent->children_count++;
  serial_printf("proc: parent=%s(pid=%u) forked child pid=%u\n",
                parent->name,
                parent->pid,
                child->pid);
}

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

  proc_shm_detach_all(proc);
  proc_release_user_memory(proc);
  proc_file_table_cleanup(proc);

  if(proc->address_space_root) {
    pmm_free_pages(proc->address_space_root, 1);
    proc->address_space_root = 0;
  }

  proc->cpu_state = NULL;

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

    if(proc->state == PROC_STATE_ZOMBIE) {
      continue;
    }

    if(proc->state == PROC_STATE_TERMINATED) {
      scheduler_cleanup_process(proc);
      continue;
    }

    if(proc->state == PROC_STATE_SLEEPING) {
      scheduler_sleep_enqueue(proc);
      continue;
    }

    if(proc->state == PROC_STATE_STOPPED) {
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

  proc_shm_detach_all(proc);
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

  proc->cpu_state = NULL;

  kfree(proc);
}

void proc_debug(cpu_state_p state) {
  serial_printf("proc_debug: %p\n", state);
  serial_printf("            cs:  %lx ss:  %lx rfl: %lx\n", state->cs, state->ss, state->rflags);
  serial_printf("proc_debug: rbp: %lx rdi: %lx rip: %lx rsi: %lx\n", state->rbp, state->rdi, state->rip, state->rsi);
  serial_printf("proc_debug: rsp: %lx rax: %lx rbx: %lx rcx: %lx\n", state->rsp, state->rax, state->rbx, state->rcx);
  serial_printf("proc_debug: rdx: %lx r8:  %lx r9:  %lx r10: %lx\n", state->rdx, state->r8, state->r9, state->r10);
}

cpu_state_p proc_switch(cpu_state_p frame) {
  if(current == NULL) {
    current = &kernel_process_info;
  }

  uint64_t now = scheduler_now_us();

  if(last_exec == 0) {
    last_exec = now;
  }

  uint64_t diff = now - last_exec;
  last_exec = now;

  if(current != NULL) {
    if(current->cpu_state != NULL) {
  serial_printf("proc_switch: save pid=%u prev_rax=%lx frame->rax=%lx\n",
                current->pid,
                current->cpu_state ? current->cpu_state->rax : 0xffffffffffffffffull,
                ((cpu_state_t*)frame)->rax);
    }
    proc_signal_handle_pending(current, frame);
  }

  if(current && current->cpu_state) {
    memcpy(current->cpu_state, frame, sizeof(cpu_state_t));
    current->kernel_rsp = (uint64_t)frame;
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
      return frame;
    }
  }

  proc_info_p previous = current;

  if(previous != NULL) {
    if(to_sleep) {
      previous->state = PROC_STATE_SLEEPING;
      scheduler_sleep_enqueue(previous);
    } else if(previous->state == PROC_STATE_ZOMBIE) {
      // keep zombie around for waitpid
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

  while(true) {
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

    proc_signal_delivery_t delivery =
      proc_signal_handle_pending(current, current->cpu_state);

    if(delivery == PROC_SIGNAL_DELIVERY_STOPPED &&
       current != &kernel_process_info) {
      continue;
    }

    if(delivery == PROC_SIGNAL_DELIVERY_TERMINATED &&
       current != &kernel_process_info) {
      if(current->state == PROC_STATE_TERMINATED) {
        scheduler_cleanup_process(current);
      }
      continue;
    }

    if(current->state == PROC_STATE_ZOMBIE ||
       current->state == PROC_STATE_TERMINATED) {
      if(current != &kernel_process_info) {
        if(current->state == PROC_STATE_TERMINATED) {
          scheduler_cleanup_process(current);
        }
        continue;
      }
    }

    break;
  }

  current->state = PROC_STATE_RUNNING;
  current->dispatch_count++;
  current->time_slice_remaining_us = current->quantum_us;
  current->last_dispatch_us = now;

  phys_addr_t desired_cr3 = current->address_space_root ? current->address_space_root : pmm_get_kernel_cr3();
  if(read_cr3() != desired_cr3) {
    write_cr3(desired_cr3);
  }

  serial_printf("proc_switch: restore pid=%u cpu_state->rax=%lx\n",
                current->pid,
                current->cpu_state->rax);
  serial_printf("proc_switch: resume frame=%p\n", (void*)current->cpu_state);

  uint64_t kernel_stack = proc_kernel_stack_top(current);
  if(kernel_stack != 0) {
    tss_update_kernel_stack(kernel_stack);
    syscall_set_kernel_stack(kernel_stack);
  }

  cpu_state_p next_frame = current->cpu_state;
  uint64_t next_rsp = current->kernel_rsp ? current->kernel_rsp : (uint64_t)next_frame;
  uint64_t dummy_prev = (uint64_t)frame;
  uint64_t* prev_slot = previous ? &previous->kernel_rsp : &dummy_prev;
  context_switch(prev_slot, next_rsp);
  return next_frame;
}

void proc_create(proc_info_p proc, const char* name, void (*entrypoint)(void *), void* arg) {
  memset(proc->name, 0, sizeof(proc->name));
  strncpy(proc->name, name, sizeof(proc->name) - 1);
  proc->first_child = NULL;
  proc->sibling_next = NULL;
  proc->children_count = 0;
  proc->parent = current;
  proc->pid = last_pid++;
  proc_file_table_init(proc);
  if(current != NULL) {
    proc_file_table_clone(proc, current);
  }
  proc_signal_state_init(proc);
  proc->stopped = false;
  proc->stop_status_pending = false;
  proc->stop_status = 0;

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
  if(current != NULL && current->cwd_len > 0) {
    size_t copy_len = current->cwd_len < PROC_CWD_MAX - 1 ? current->cwd_len : PROC_CWD_MAX - 1;
    memcpy(proc->cwd, current->cwd, copy_len);
    proc->cwd[copy_len] = '\0';
    proc->cwd_len = copy_len;
  } else {
    proc->cwd[0] = '/';
    proc->cwd[1] = '\0';
    proc->cwd_len = 1;
  }
  proc->dispatch_count = 0;
  proc->exec_time = 0;
  proc->last_dispatch_us = 0;
  proc->waitpid_target = -1;
  proc->waitpid_waiting = false;

  proc->cpu_state = (cpu_state_t*)((uint8_t*)proc->stack_base + PROC_STACK_SIZE - sizeof(cpu_state_t));
  memset(proc->cpu_state, 0, sizeof(cpu_state_t));
  proc->kernel_rsp = (uint64_t)proc->cpu_state;
  proc->cpu_state->rip = (uint64_t)entrypoint;
  proc->cpu_state->rdi = (uint64_t)arg;
  proc->cpu_state->rsp = (uint64_t)proc->stack_base + PROC_STACK_SIZE;
  proc->cpu_state->rbp = proc->cpu_state->rsp;
  proc->cpu_state->rflags = 0x246;
  uint16_t cs = KERNEL_CODE_SEGMENT;
  uint16_t ss = KERNEL_DATA_SEGMENT;
  if(current != NULL && current->cpu_state != NULL) {
    cs = current->cpu_state->cs;
    ss = current->cpu_state->ss;
  }
  proc->cpu_state->cs = cs;
  proc->cpu_state->ss = ss;
  proc->address_space_root = pmm_get_kernel_cr3();
  proc->user_segment_count = 0;
  proc->shm_attachment_count = 0;
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
  (void)pid;
  return 0x0000000040000000ull; // 1 GiB — keep within 32-bit syscall return range
}

static inline virt_addr_t user_mmap_limit(uint32_t pid) {
  (void)pid;
  return 0x0000000080000000ull; // 2 GiB window for mmap allocations
}

static inline bool segment_can_extend(const proc_user_segment_t* segment,
                                      phys_addr_t phys) {
  if(segment == NULL) {
    return false;
  }

  phys_addr_t expected = segment->phys + (phys_addr_t)segment->pages * PAGE_SIZE;
  return expected == phys;
}

static bool proc_register_user_segment_internal(proc_info_p proc, phys_addr_t phys, size_t pages) {
  if(pages == 0 || phys == 0) {
    return false;
  }

  if(proc->user_segment_count > 0) {
    proc_user_segment_t* last = &proc->user_segments[proc->user_segment_count - 1];
    if(segment_can_extend(last, phys)) {
      if(last->pages > SIZE_MAX - pages) {
        return false;
      }
      last->pages += pages;
      return true;
    }
  }

  if(proc->user_segment_count >= PROC_MAX_USER_SEGMENTS) {
    return false;
  }

  proc_user_segment_t* slot = &proc->user_segments[proc->user_segment_count++];
  slot->phys = phys;
  slot->pages = pages;
  return true;
}

bool proc_register_user_segment(proc_info_p proc, phys_addr_t phys, size_t pages) {
  return proc_register_user_segment_internal(proc, phys, pages);
}

static void proc_collapse_segment(proc_info_p proc, size_t index) {
  if(proc == NULL || index >= proc->user_segment_count) {
    return;
  }

  for(size_t j = index + 1; j < proc->user_segment_count; ++j) {
    proc->user_segments[j - 1] = proc->user_segments[j];
  }
  proc->user_segment_count--;
}

void proc_unregister_user_segment(proc_info_p proc, phys_addr_t phys, size_t pages) {
  if(proc == NULL || pages == 0) {
    return;
  }

  const phys_addr_t bytes = (phys_addr_t)pages * PAGE_SIZE;
  const phys_addr_t remove_start = phys;
  const phys_addr_t remove_end = remove_start + bytes;

  for(size_t i = 0; i < proc->user_segment_count; ++i) {
    proc_user_segment_t* segment = &proc->user_segments[i];
    const phys_addr_t seg_start = segment->phys;
    const phys_addr_t seg_end = seg_start + (phys_addr_t)segment->pages * PAGE_SIZE;

    if(remove_start < seg_start || remove_end > seg_end) {
      continue;
    }

    if(remove_start == seg_start && remove_end == seg_end) {
      proc_collapse_segment(proc, i);
      return;
    }

    if(remove_start == seg_start) {
      segment->phys = remove_end;
      segment->pages -= pages;
      return;
    }

    if(remove_end == seg_end) {
      segment->pages -= pages;
      return;
    }

    const phys_addr_t prefix_bytes = remove_start - seg_start;
    const size_t prefix_pages = (size_t)(prefix_bytes / PAGE_SIZE);
    const size_t suffix_pages = segment->pages - prefix_pages - pages;

    segment->pages = prefix_pages;

    if(suffix_pages == 0) {
      return;
    }

    if(proc->user_segment_count >= PROC_MAX_USER_SEGMENTS) {
      // Cannot record the suffix separately; leak prevention takes precedence.
      return;
    }

    for(size_t j = proc->user_segment_count; j > i + 1; --j) {
      proc->user_segments[j] = proc->user_segments[j - 1];
    }

    proc_user_segment_t* tail = &proc->user_segments[i + 1];
    tail->phys = remove_end;
    tail->pages = suffix_pages;
    proc->user_segment_count++;
    return;
  }
}

bool proc_shm_track_attachment(proc_info_p proc,
                               shm_region_t* region,
                               virt_addr_t base,
                               size_t length,
                               int shmid,
                               int flags) {
  if(proc == NULL || region == NULL || length == 0) {
    return false;
  }

  if(proc->shm_attachment_count >= PROC_MAX_SHM_ATTACHMENTS) {
    return false;
  }

  proc_shm_attachment_t* slot = &proc->shm_attachments[proc->shm_attachment_count++];
  slot->region = region;
  slot->base = base;
  slot->length = length;
  slot->shmid = shmid;
  slot->flags = flags;
  shm_region_increment_attachments(region);
  return true;
}

bool proc_shm_detach(proc_info_p proc, virt_addr_t base) {
  if(proc == NULL) {
    return false;
  }

  size_t index = 0;
  bool found = false;
  proc_shm_attachment_t attachment;

  for(; index < proc->shm_attachment_count; ++index) {
    if(proc->shm_attachments[index].base == base) {
      attachment = proc->shm_attachments[index];
      found = true;
      break;
    }
  }

  if(!found) {
    return false;
  }

  if(!vm_unmap(proc, attachment.base, attachment.length)) {
    return false;
  }

  for(size_t j = index + 1; j < proc->shm_attachment_count; ++j) {
    proc->shm_attachments[j - 1] = proc->shm_attachments[j];
  }
  proc->shm_attachment_count--;

  shm_region_decrement_attachments(attachment.region);
  shm_region_unref(attachment.region);
  return true;
}

bool proc_shm_remove_attachment(proc_info_p proc,
                                virt_addr_t base,
                                proc_shm_attachment_t* out) {
  if(proc == NULL) {
    return false;
  }

  for(size_t i = 0; i < proc->shm_attachment_count; ++i) {
    if(proc->shm_attachments[i].base == base) {
      if(out) {
        *out = proc->shm_attachments[i];
      }
      for(size_t j = i + 1; j < proc->shm_attachment_count; ++j) {
        proc->shm_attachments[j - 1] = proc->shm_attachments[j];
      }
      proc->shm_attachment_count--;
      return true;
    }
  }
  return false;
}

void proc_shm_detach_all(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  while(proc->shm_attachment_count > 0) {
    proc_shm_attachment_t attachment =
      proc->shm_attachments[proc->shm_attachment_count - 1];
    if(!proc_shm_detach(proc, attachment.base)) {
      break;
    }
  }
}

bool proc_shm_inherit(proc_info_p child, proc_info_p parent) {
  if(child == NULL || parent == NULL) {
    return false;
  }

  size_t original_count = child->shm_attachment_count;

  for(size_t i = 0; i < parent->shm_attachment_count; ++i) {
    proc_shm_attachment_t* attachment = &parent->shm_attachments[i];
    uint32_t flags = VM_REGION_FLAG_USER | VM_REGION_FLAG_READ;
    bool writable = (attachment->flags & SHM_RDONLY) == 0;
    if(writable) {
      flags |= VM_REGION_FLAG_WRITE;
    }

    shm_region_t* region = attachment->region;
    shm_region_ref(region);

    if(!vm_map_shared(child, region, attachment->base, flags, writable)) {
      shm_region_unref(region);
      goto inherit_fail;
    }

    if(!proc_shm_track_attachment(child,
                                  region,
                                  attachment->base,
                                  attachment->length,
                                  attachment->shmid,
                                  attachment->flags)) {
      vm_unmap(child, attachment->base, attachment->length);
      shm_region_unref(region);
      goto inherit_fail;
    }

    virt_addr_t end = attachment->base + attachment->length;
    virt_addr_t next = (end + PAGE_SIZE - 1) & ~((virt_addr_t)PAGE_SIZE - 1);
    if(next > child->mmap_next) {
      child->mmap_next = next;
    }
  }

  return true;

inherit_fail:
  while(child->shm_attachment_count > original_count) {
    proc_shm_attachment_t rollback = child->shm_attachments[child->shm_attachment_count - 1];
    vm_unmap(child, rollback.base, rollback.length);
    child->shm_attachment_count--;
    shm_region_decrement_attachments(rollback.region);
    shm_region_unref(rollback.region);
  }
  return false;
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
    scheduler_actions |= SCHED_ACTION_FORCE;
    return;
  }

  current->sleep_until = scheduler_now_us() + duration_us;
  current->time_slice_remaining_us = 0;
  scheduler_actions |= (SCHED_ACTION_SLEEP | SCHED_ACTION_FORCE);
  serial_printf("proc_request_sleep: pid=%u duration=%lu\n",
                current->pid,
                (unsigned long)duration_us);
  serial_printf("proc_request_sleep: caller=%lx\n",
                (unsigned long)__builtin_return_address(0));
}

void proc_request_block(void) {
  if(current == NULL) {
    return;
  }

  current->sleep_until = UINT64_MAX;
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

  sleep_queue_remove(proc);
  proc->state = PROC_STATE_READY;
  proc->time_slice_remaining_us = proc->quantum_us;
  proc->sleep_until = 0;
  ready_queue_push(proc);

  scheduler_actions |= SCHED_ACTION_FORCE;

  if(flags & (1ull << 9)) {
    enable_interrupts();
  }
}

void proc_mark_stopped(proc_info_p proc, int signo) {
  if(proc == NULL || proc == &kernel_process_info) {
    return;
  }

  proc->stop_status = encode_stopped_status(signo);
  proc->stop_status_pending = true;
  proc->stopped = true;
  proc->continue_status = 0;
  proc->continued_pending = false;

  if(proc->state == PROC_STATE_SLEEPING) {
    sleep_queue_remove(proc);
  } else if(proc->state == PROC_STATE_READY) {
    ready_queue_remove(proc);
  }

  proc->state = PROC_STATE_STOPPED;
  proc->time_slice_remaining_us = 0;

  proc_info_p parent = proc->parent;
  if(parent != NULL && parent->waitpid_waiting) {
    if(parent->waitpid_target == -1 || parent->waitpid_target == (int)proc->pid) {
      parent->waitpid_waiting = false;
      parent->waitpid_target = -1;
      proc_mark_ready(parent);
    }
  }

  scheduler_actions |= SCHED_ACTION_FORCE;
}

void proc_mark_continued(proc_info_p proc) {
  if(proc == NULL || proc == &kernel_process_info) {
    return;
  }

  proc->stopped = false;
  proc->stop_status_pending = false;
  proc->continue_status = 0xffff;
  proc->continued_pending = true;

  if(proc->state == PROC_STATE_STOPPED) {
    proc_mark_ready(proc);
  }

  proc_info_p parent = proc->parent;
  if(parent != NULL && parent->waitpid_waiting) {
    if(parent->waitpid_target == -1 || parent->waitpid_target == (int)proc->pid) {
      parent->waitpid_waiting = false;
      parent->waitpid_target = -1;
      proc_mark_ready(parent);
    }
  }
}

proc_info_p proc_fork(proc_info_p parent, const syscall_frame_t* frame, int* err_out) {
  if(err_out) {
    *err_out = -ENOMEM;
  }

  if(parent == NULL || frame == NULL) {
    return NULL;
  }

  serial_printf("proc_fork: parent pid=%u entering\n", parent->pid);
  proc_info_p child = kmalloc(sizeof(proc_info_t));
  if(child == NULL) {
    serial_printf("proc_fork: kmalloc proc_info failed\n");
    return NULL;
  }
  memset(child, 0, sizeof(proc_info_t));
  proc_file_table_init(child);
  proc_file_table_clone(child, parent);
  proc_signal_state_copy(child, parent);
  serial_printf("proc_fork: proc_info allocated\n");

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
  child->err_no = 0;
  child->exit_code = 0;
  child->sleep_until = 0;
  child->last_dispatch_us = 0;
  child->dispatch_count = 0;
  child->exec_time = 0;
  child->waitpid_target = -1;
  child->waitpid_waiting = false;

  child->stack_pointer = kmalloc(PROC_STACK_SIZE + 0xF);
  if(child->stack_pointer == NULL) {
    serial_printf("proc_fork: stack allocation failed\n");
    proc_free_resources(child);
    return NULL;
  }
  uintptr_t aligned = ((uintptr_t)child->stack_pointer + 0xF) & ~((uintptr_t)0xF);
  child->stack_base = (uintptr_t*)aligned;
  memset(child->stack_base, 0, PROC_STACK_SIZE);
  serial_printf("proc_fork: stack allocated aligned=%p\n", child->stack_base);

  child->cpu_state = (cpu_state_t*)((uint8_t*)child->stack_base + PROC_STACK_SIZE - sizeof(cpu_state_t));
  memset(child->cpu_state, 0, sizeof(cpu_state_t));
  child->kernel_rsp = (uint64_t)child->cpu_state;
  serial_printf("proc_fork: cpu_state allocated\n");

  phys_addr_t new_root = pmm_clone_kernel_address_space();
  if(new_root == 0) {
    serial_printf("proc_fork: clone address space failed\n");
    proc_free_resources(child);
    return NULL;
  }
  child->address_space_root = new_root;
  serial_printf("proc_fork: new address space=%lx\n", (unsigned long)new_root);

  child->user_segment_count = 0;
  child->shm_attachment_count = 0;
  child->vm_region_count = 0;

  if(!vm_clone(child, parent)) {
    serial_printf("proc_fork: vm_clone failed\n");
    proc_free_resources(child);
    return NULL;
  }
  serial_printf("proc_fork: vm_clone complete\n");

  if(!proc_shm_inherit(child, parent)) {
    serial_printf("proc_fork: shared memory inheritance failed\n");
    proc_free_resources(child);
    return NULL;
  }

  memcpy(child->cpu_state, frame, sizeof(syscall_frame_t));
  child->cpu_state->rax = 0;
  serial_printf("proc_fork: child pid=%u rax=%lu rip=%lx\n",
                child->pid,
                child->cpu_state->rax,
                child->cpu_state->rip);

  proc_set_priority(child, parent->priority);
  child->time_slice_remaining_us = child->quantum_us;
  child->state = PROC_STATE_READY;

  proc_link_child(parent, child);
  serial_printf("proc_fork: linked child pid=%u\n", child->pid);

  proc_execute(child);
  serial_printf("proc_fork: scheduled child pid=%u\n", child->pid);

  if(err_out) {
    *err_out = 0;
  }

  return child;
}

static bool proc_setup_exec_stack(proc_info_p proc,
                                 syscall_frame_t* frame,
                                 const proc_exec_args_t* args);

int proc_exec_image(proc_info_p proc,
                    const uint8_t* image,
                    size_t size,
                    syscall_frame_t* frame,
                    const proc_exec_args_t* args) {
  if(proc == NULL || image == NULL || size == 0 || frame == NULL) {
    return -EINVAL;
  }

  serial_printf("proc_exec_image: pid=%u size=%lu\n", proc->pid, (unsigned long)size);

  uint8_t* elf_copy = kmalloc(size);
  if(elf_copy == NULL) {
    serial_printf("proc_exec_image: elf_copy allocation failed\n");
    return -ENOMEM;
  }

  memcpy(elf_copy, image, size);
  serial_printf("proc_exec_image: elf copied\n");

  phys_addr_t new_root = pmm_clone_kernel_address_space();
  if(new_root == 0) {
    kfree(elf_copy);
    serial_printf("proc_exec_image: clone address space failed\n");
    return -ENOMEM;
  }
  serial_printf("proc_exec_image: new_root=%lx\n", (unsigned long)new_root);

  proc_info_t* staging = kmalloc(sizeof(proc_info_t));
  if(staging == NULL) {
    pmm_free_pages(new_root, 1);
    kfree(elf_copy);
    serial_printf("proc_exec_image: staging allocation failed\n");
    return -ENOMEM;
  }

  memset(staging, 0, sizeof(*staging));
  staging->pid = proc->pid;
  staging->address_space_root = new_root;

  proc_file_table_prepare_exec(proc);
  serial_printf("proc_exec_image: file table prepared\n");

  virt_addr_t stack_top = user_stack_top(proc->pid);
  virt_addr_t stack_base_vaddr = stack_top - PROC_USER_STACK_SIZE;
  staging->user_stack_base_vaddr = stack_base_vaddr;
  staging->user_stack_size = PROC_USER_STACK_SIZE;

  int result = -ENOMEM;

  if(!vm_region_add(staging,
                    stack_base_vaddr,
                    PROC_USER_STACK_SIZE,
                    VM_REGION_STACK,
                    VM_REGION_FLAG_READ | VM_REGION_FLAG_WRITE | VM_REGION_FLAG_USER | VM_REGION_FLAG_GROW_DOWN)) {
    serial_printf("proc_exec_image: vm_region_add stack failed\n");
    goto fail;
  }
  serial_printf("proc_exec_image: stack region added\n");

  vm_region_t* stack_region = vm_region_find(staging, stack_base_vaddr);
  if(stack_region == NULL) {
    goto fail;
  }

  phys_addr_t initial_stack_phys = pmm_alloc_pages(1);
  if(initial_stack_phys == 0) {
    serial_printf("proc_exec_image: initial stack alloc failed\n");
    goto fail;
  }
  serial_printf("proc_exec_image: initial stack phys=%lx\n", (unsigned long)initial_stack_phys);

  void* stack_page_ptr = (void*)physical_to_virtual(initial_stack_phys);
  memset(stack_page_ptr, 0, PAGE_SIZE);

  virt_addr_t initial_stack_page = stack_top - PAGE_SIZE;
  if(!pmm_map_page_in_root(new_root, initial_stack_page, initial_stack_phys, true, true)) {
    pmm_free_pages(initial_stack_phys, 1);
    serial_printf("proc_exec_image: map initial stack failed\n");
    goto fail;
  }
  serial_printf("proc_exec_image: initial stack mapped\n");

  if(!proc_register_user_segment(staging, initial_stack_phys, 1)) {
    pmm_unmap_page_in_root(new_root, initial_stack_page);
    pmm_free_pages(initial_stack_phys, 1);
    serial_printf("proc_exec_image: register stack segment failed\n");
    goto fail;
  }
  serial_printf("proc_exec_image: stack segment registered\n");

  vm_region_note_mapping(stack_region, initial_stack_page, PAGE_SIZE);

  uint64_t entry = 0;
  if(!elf64_load_image(staging, new_root, elf_copy, size, &entry)) {
    serial_printf("proc_exec_image: elf64_load_image failed\n");
    result = -ENOEXEC;
    goto fail;
  }
  serial_printf("proc_exec_image: elf loaded entry=%lx\n", entry);

  proc->brk = 0;
  proc->heap = 0;

  phys_addr_t old_root = proc->address_space_root;
  proc_shm_detach_all(proc);
  proc_release_user_memory(proc);

  if(old_root != 0 && old_root != new_root && old_root != pmm_get_kernel_cr3()) {
    pmm_free_pages(old_root, 1);
  }

  proc->address_space_root = new_root;
  proc->user_stack_base_vaddr = staging->user_stack_base_vaddr;
  proc->user_stack_size = staging->user_stack_size;
  proc->user_segment_count = staging->user_segment_count;
  memcpy(proc->user_segments,
         staging->user_segments,
         staging->user_segment_count * sizeof(proc_user_segment_t));
  proc->vm_region_count = staging->vm_region_count;
  memcpy(proc->vm_regions,
         staging->vm_regions,
         staging->vm_region_count * sizeof(vm_region_t));
  proc->shm_attachment_count = 0;
  proc->user_mode = true;
  proc->mmap_base = user_mmap_base(proc->pid);
  proc->mmap_next = proc->mmap_base;
  proc->mmap_limit = user_mmap_limit(proc->pid);

  kfree(elf_copy);
  elf_copy = NULL;

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
      serial_printf("proc_exec_image: cpu_state allocation failed\n");
      result = -ENOMEM;
      goto fail;
    }
  }

  if(!proc_setup_exec_stack(proc, frame, args)) {
    result = -EFAULT;
    serial_printf("proc_exec_image: setup_exec_stack failed\n");
    goto fail;
  }

  memcpy(proc->cpu_state, frame, sizeof(syscall_frame_t));
  serial_printf("proc_exec_image: completed successfully\n");

  kfree(staging);

  return 0;

fail:
  if(staging != NULL) {
    proc_release_user_memory(staging);
  }
  if(new_root != 0) {
    pmm_free_pages(new_root, 1);
  }
  if(elf_copy != NULL) {
    kfree(elf_copy);
  }
  if(staging != NULL) {
    kfree(staging);
  }
  serial_printf("proc_exec_image: failing with result=%d\n", result);
  return result;
}

static bool proc_setup_exec_stack(proc_info_p proc,
                                 syscall_frame_t* frame,
                                 const proc_exec_args_t* args) {
  if(proc == NULL || frame == NULL) {
    return false;
  }

  if(args == NULL) {
    frame->rdi = 0;
    frame->rsi = 0;
    frame->rdx = 0;
    return true;
  }

  size_t argc = args->argc;
  size_t envc = args->envc;
  char** argv = args->argv;
  char** envp = args->envp;

  uintptr_t sp = frame->rsp;
  uintptr_t stack_base = proc->user_stack_base_vaddr;

  uintptr_t* argv_ptrs = NULL;
  uintptr_t* envp_ptrs = NULL;

  if(argc > 0) {
    argv_ptrs = kmalloc(sizeof(uintptr_t) * argc);
    if(argv_ptrs == NULL) {
      return false;
    }
  }

  if(envc > 0) {
    envp_ptrs = kmalloc(sizeof(uintptr_t) * envc);
    if(envp_ptrs == NULL) {
      kfree(argv_ptrs);
      return false;
    }
  }

  for(size_t i = 0; i < envc; i++) {
    const char* str = envp ? envp[i] : NULL;
    if(str == NULL) {
      envp_ptrs[i] = 0;
      continue;
    }
    size_t len = strlen(str) + 1;
    if(sp < stack_base + len) {
      if(argv_ptrs) {
        kfree(argv_ptrs);
      }
      if(envp_ptrs) {
        kfree(envp_ptrs);
      }
      return false;
    }
    sp -= len;
    memcpy((void*)sp, str, len);
    envp_ptrs[i] = sp;
  }

  for(size_t i = 0; i < argc; i++) {
    const char* str = argv ? argv[i] : NULL;
    if(str == NULL) {
      argv_ptrs[i] = 0;
      continue;
    }
    size_t len = strlen(str) + 1;
    if(sp < stack_base + len) {
      if(argv_ptrs) {
        kfree(argv_ptrs);
      }
      if(envp_ptrs) {
        kfree(envp_ptrs);
      }
      return false;
    }
    sp -= len;
    memcpy((void*)sp, str, len);
    argv_ptrs[i] = sp;
  }

  sp &= ~((uintptr_t)0xf);

  if(sp < stack_base + sizeof(uintptr_t)) {
    if(argv_ptrs) {
      kfree(argv_ptrs);
    }
    if(envp_ptrs) {
      kfree(envp_ptrs);
    }
    return false;
  }

  sp -= sizeof(uintptr_t);
  *((uintptr_t*)sp) = 0;
  for(size_t i = envc; i > 0; i--) {
    sp -= sizeof(uintptr_t);
    *((uintptr_t*)sp) = envp_ptrs ? envp_ptrs[i - 1] : 0;
  }
  uintptr_t envp_user = sp;

  sp -= sizeof(uintptr_t);
  *((uintptr_t*)sp) = 0;
  for(size_t i = argc; i > 0; i--) {
    sp -= sizeof(uintptr_t);
    *((uintptr_t*)sp) = argv_ptrs ? argv_ptrs[i - 1] : 0;
  }
  uintptr_t argv_user = sp;

  sp &= ~((uintptr_t)0xf);

  sp -= sizeof(uintptr_t);
  *((uintptr_t*)sp) = argc;

  frame->rsp = sp;
  frame->rdi = argc;
  frame->rsi = argv_user;
  frame->rdx = envp_user;
  serial_printf("proc_setup_exec_stack: argc=%lu argv=%lx envp=%lx sp=%lx\n",
                (unsigned long)argc,
                (unsigned long)argv_user,
                (unsigned long)envp_user,
                (unsigned long)sp);

  if(argv_ptrs) {
    kfree(argv_ptrs);
  }
  if(envp_ptrs) {
    kfree(envp_ptrs);
  }

  return true;
}

int proc_waitpid(proc_info_p parent, int pid, int options, int* status_out) {
  if(parent == NULL) {
    return -EINVAL;
  }

  proc_info_p prev = NULL;
  proc_info_p child = parent->first_child;
  bool match_found = false;
  bool report_stopped = (options & WUNTRACED) != 0;
  bool report_continued = (options & WCONTINUED) != 0;

  if(child == NULL) {
    return -ECHILD;
  }

  while(child != NULL) {
    proc_info_p next = child->sibling_next;
    bool matches = (pid == -1) || (child->pid == (uint32_t)pid);
    if(matches) {
      match_found = true;

      if(child->stop_status_pending) {
        if(report_stopped) {
          if(status_out) {
            *status_out = child->stop_status;
          }
          child->stop_status_pending = false;
          parent->waitpid_waiting = false;
          parent->waitpid_target = -1;
          return (int)child->pid;
        }
      }

      if(child->continued_pending) {
        if(report_continued) {
          if(status_out) {
            *status_out = child->continue_status;
          }
          child->continued_pending = false;
          child->continue_status = 0;
          parent->waitpid_waiting = false;
          parent->waitpid_target = -1;
          return (int)child->pid;
        }
      }

      if(child->state == PROC_STATE_ZOMBIE) {
        int exit_code = child->exit_code;
        if(status_out) {
          *status_out = exit_code;
        }

        if(prev) {
          prev->sibling_next = child->sibling_next;
        } else {
          parent->first_child = child->sibling_next;
        }

        if(parent->children_count > 0) {
          parent->children_count--;
        }

        child->parent = NULL;
        child->sibling_next = NULL;
        child->state = PROC_STATE_TERMINATED;

        int reaped_pid = (int)child->pid;
        scheduler_cleanup_process(child);
        return reaped_pid;
      }

      if(pid > 0) {
        break;
      }
    }

    prev = child;
    child = next;
  }

  if(pid > 0 && !match_found) {
    return -ECHILD;
  }

  return 0;
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
  proc_signal_state_init(current);
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
    syscall_set_kernel_stack(kernel_stack);
  }
  register_timer_callback(proc_switch);
  printf(".OK\n");
}

static inline int encode_exit_status(int code) {
  return (code & 0xff) << 8;
}

static inline int encode_signal_status(int signo) {
  return signo & 0x7f;
}

static void proc_exit_with_status(int status) {
  current->state = PROC_STATE_ZOMBIE;
  current->exit_code = status;
  current->stopped = false;
  current->stop_status_pending = false;
  current->continued_pending = false;
  serial_printf("proc_exit: Process %s exited with status %d\n", current->name, status);

  proc_info_p parent = current->parent;
  if(parent != NULL && parent->waitpid_waiting) {
    if(parent->waitpid_target == -1 || parent->waitpid_target == (int)current->pid) {
      parent->waitpid_waiting = false;
      parent->waitpid_target = -1;
      proc_mark_ready(parent);
    }
  }

  scheduler_actions |= SCHED_ACTION_FORCE;
}

void proc_exit(int code) {
  proc_exit_with_status(encode_exit_status(code));
}

void proc_exit_signal(int signo) {
  proc_exit_with_status(encode_signal_status(signo));
}

int proc_kill_pid(uint32_t pid, int code) {
  proc_info_p target = proc_find_by_pid(pid);
  if(target == NULL || target == &kernel_process_info) {
    return -ESRCH;
  }

  if(target->state == PROC_STATE_ZOMBIE || target->state == PROC_STATE_TERMINATED) {
    return -ESRCH;
  }

  target->exit_code = code;
  target->state = PROC_STATE_ZOMBIE;
  target->stopped = false;
  target->stop_status_pending = false;
  target->continued_pending = false;
  proc_info_p parent = target->parent;
  if(parent != NULL && parent->waitpid_waiting) {
    if(parent->waitpid_target == -1 || parent->waitpid_target == (int)target->pid) {
      parent->waitpid_waiting = false;
      parent->waitpid_target = -1;
      proc_mark_ready(parent);
    }
  }

  scheduler_actions |= SCHED_ACTION_FORCE;
  return 0;
}
