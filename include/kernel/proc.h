#ifndef MENIOS_INCLUDE_KERNEL_PROC_H
#define MENIOS_INCLUDE_KERNEL_PROC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <types.h>
#include <kernel/file.h>
#include <kernel/vm_region.h>
#include <kernel/signal.h>

struct syscall_frame_t;

#define PROC_KERNEL 0
#define PROC_MAX   16

/*
                               DISPATCH                                   
                              +------------+                              
                              |            |                              
+------------+     +----------+-+       +--v---------+EXIT  +------------+
| NEW        +-----> READY      |       | RUNNING    +------> TERMINATED |
+------------+     +--^-------^-+       +--+-------+-+      +------------+
                      |       |            |       |                      
                      |       |            |       |                      
                      |       +------------+       |                      
                      |        INTERRUPT           |                      
                      |                            |                      
         I/O COMPLETED|       +------------+       |I/O WAIT              
                      +-------+ WAITING    <-------+                      
                              +------------+                              
 */

#define PROC_STATE_NEW        0
#define PROC_STATE_READY      1
#define PROC_STATE_RUNNING    2
#define PROC_STATE_WAITING    3
#define PROC_STATE_SLEEPING   4
#define PROC_STATE_ZOMBIE     5
#define PROC_STATE_STOPPED    6
#define PROC_STATE_TERMINATED 7

#define PROC_PRIO_IDLE    0
#define PROC_PRIO_LOW     1
#define PROC_PRIO_NORMAL  2
#define PROC_PRIO_HIGH    3
#define PROC_PRIO_REALTIME 4
#define PROC_PRIORITY_MAX PROC_PRIO_REALTIME
#define PROC_PRIORITY_COUNT (PROC_PRIORITY_MAX + 1)

#define PROC_PARENT_NONE 0

#define RLIMIT_DATA (4 * 1024 * 1024)

#define PROC_STACK_SIZE (64 * 1024)
#define PROC_USER_STACK_SIZE (1 * 1024 * 1024)
#define PROC_MAX_USER_SEGMENTS 4096
#define PROC_MAX_VM_REGIONS 32
#define PROC_CWD_MAX 256

typedef struct proc_user_segment_t {
  phys_addr_t phys;
  size_t      pages;
} proc_user_segment_t;

struct shm_region;
typedef struct shm_region shm_region_t;

typedef struct proc_shm_attachment_t {
  shm_region_t* region;
  virt_addr_t   base;
  size_t        length;
  int           shmid;
  int           flags;
} proc_shm_attachment_t;

#define PROC_MAX_SHM_ATTACHMENTS 32

typedef struct proc_itimer_t {
  bool     active;
  uint64_t expires_us;
  uint64_t interval_us;
} proc_itimer_t;

#define PROC_ITIMER_REAL 0
#define PROC_ITIMER_VIRTUAL 1
#define PROC_ITIMER_PROF 2
#define PROC_ITIMER_MAX 3

typedef struct cpu_state_t {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
  uint64_t rip;   
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
} __attribute__((packed)) cpu_state_t;

typedef cpu_state_t* cpu_state_p;

typedef uint8_t proc_state_t;

typedef struct proc_info_t proc_info_t;
typedef proc_info_t* proc_info_p;

typedef struct proc_info_t {
  proc_info_p  parent;
  uint32_t     children_count;
  uint32_t     pid;
  uintptr_t    brk;
  uintptr_t    heap;
  virt_addr_t  mmap_base;
  virt_addr_t  mmap_next;
  virt_addr_t  mmap_limit;
  char         cwd[PROC_CWD_MAX];
  size_t       cwd_len;
  proc_state_t state;
  uint8_t      priority;
  uint64_t     sleep_until;
  uint64_t     quantum_us;
  uint64_t     time_slice_remaining_us;
  uint64_t     last_dispatch_us;
  uint64_t     dispatch_count;
  int          waitpid_target;
  bool         waitpid_waiting;
  proc_info_p  next;
  int          exit_code;
  int          stop_status;
  int          continue_status;
  int          err_no;
  uint64_t     exec_time;
  char         name[32];
  proc_info_p  first_child;
  proc_info_p  sibling_next;
  cpu_state_t* cpu_state;
  uint64_t     kernel_rsp;
  void*        stack_pointer;
  uintptr_t*   stack_base;
  virt_addr_t  user_stack_base_vaddr;
  size_t       user_stack_size;
  bool         user_mode;
  phys_addr_t  address_space_root;
  proc_user_segment_t user_segments[PROC_MAX_USER_SEGMENTS];
  size_t       user_segment_count;
  proc_shm_attachment_t shm_attachments[PROC_MAX_SHM_ATTACHMENTS];
  size_t       shm_attachment_count;
  vm_region_t  vm_regions[PROC_MAX_VM_REGIONS];
  size_t       vm_region_count;
  void(*entrypoint)(void*);
  void*        arguments;
  file_descriptor_entry_t files[PROC_MAX_FILES];
  uint32_t     signal_pending;
  uint32_t     signal_blocked;
  struct sigaction signal_actions[SIG_MAX];
  bool         stopped;
  bool         stop_status_pending;
  bool         continued_pending;
  bool         syscall_gs_active;
  bool         syscall_gs_needs_restore;
  proc_itimer_t timers[PROC_ITIMER_MAX];
} proc_info_t;

typedef proc_info_t* proc_info_p;

static inline uint64_t proc_kernel_stack_top(proc_info_p proc) {
  if(proc == NULL) {
    return 0;
  }

  if(proc->stack_base == NULL) {
    uint64_t rsp = 0;
#ifdef MENIOS_KERNEL
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
#endif
    return rsp;
  }

  return (uint64_t)((uintptr_t)proc->stack_base + PROC_STACK_SIZE);
}

extern proc_info_t kernel_process_info;
extern proc_info_p procs[PROC_MAX];
extern proc_info_p current;

void scheduler_init();
void proc_create(proc_info_p proc, const char* name, void (*entrypoint)(void *), void* arg);
void proc_execute(proc_info_p proc);
void proc_exit(int code);
void proc_exit_signal(int signo);
cpu_state_p proc_switch(cpu_state_p state);
void proc_create_user(proc_info_p proc, const char* name, const void* code_blob, size_t code_size, void* arg);
bool proc_register_user_segment(proc_info_p proc, phys_addr_t phys, size_t pages);
void proc_unregister_user_segment(proc_info_p proc, phys_addr_t phys, size_t pages);
void proc_set_priority(proc_info_p proc, uint8_t priority);
void scheduler_set_quantum(uint8_t priority, uint64_t quantum_us);
uint64_t scheduler_get_quantum(uint8_t priority);
void proc_request_yield(void);
void proc_request_sleep(uint64_t duration_us);
void proc_request_block(void);
void proc_mark_ready(proc_info_p proc);
void proc_mark_stopped(proc_info_p proc, int signo);
void proc_mark_continued(proc_info_p proc);
proc_info_p proc_fork(proc_info_p parent, const struct syscall_frame_t* frame, int* err_out);
int proc_waitpid(proc_info_p parent, int pid, int options, int* status_out);
proc_info_p proc_find_by_pid(uint32_t pid);
typedef struct proc_exec_args_t {
  size_t argc;
  char** argv;
  size_t envc;
  char** envp;
} proc_exec_args_t;

int proc_exec_image(proc_info_p proc,
                    const uint8_t* image,
                    size_t size,
                    struct syscall_frame_t* frame,
                    const proc_exec_args_t* args);
int proc_kill_pid(uint32_t pid, int code);
bool proc_user_buffer_accessible(proc_info_p proc, const void* ptr, size_t length);
bool proc_user_touch_range(proc_info_p proc, virt_addr_t addr, size_t length, bool write);
bool proc_user_write64(proc_info_p proc, virt_addr_t addr, uint64_t value);
bool proc_user_copy_in(proc_info_p proc, void* dest, virt_addr_t src, size_t length);
bool proc_user_copy_out(proc_info_p proc, virt_addr_t dest, const void* src, size_t length);
bool proc_shm_track_attachment(proc_info_p proc,
                               shm_region_t* region,
                               virt_addr_t base,
                               size_t length,
                               int shmid,
                               int flags);
bool proc_shm_remove_attachment(proc_info_p proc,
                                virt_addr_t base,
                                proc_shm_attachment_t* out);
void proc_shm_detach_all(proc_info_p proc);
bool proc_shm_inherit(proc_info_p child, proc_info_p parent);
bool proc_shm_detach(proc_info_p proc, virt_addr_t base);

#ifdef __cplusplus
}
#endif

#endif
