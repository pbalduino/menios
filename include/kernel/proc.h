#ifndef MENIOS_INCLUDE_KERNEL_PROC_H
#define MENIOS_INCLUDE_KERNEL_PROC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

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
#define PROC_STATE_TERMINATED 7

#define PROC_PRIO_NORMAL 3

#define PROC_PARENT_NONE 0

#define RLIMIT_DATA (4 * 1024 * 1024)

#define PROC_STACK_SIZE (16 * 1024)
#define PROC_USER_STACK_SIZE (16 * 1024)

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
  proc_state_t state;
  uint8_t      priority;
  uint64_t     sleep_until;
  proc_info_p  next;
  int          exit_code;
  int          errno;
  uint64_t     exec_time;
  char         name[32];
  proc_info_p* children;
  cpu_state_t* cpu_state;
  void*        stack_pointer;
  uintptr_t*   stack_base;
  phys_addr_t  user_stack_phys;
  size_t       user_stack_pages;
  virt_addr_t  user_stack_base_vaddr;
  size_t       user_stack_size;
  phys_addr_t  user_code_phys;
  size_t       user_code_pages;
  virt_addr_t  user_code_vaddr;
  bool         user_mode;
  phys_addr_t  address_space_root;
  void(*entrypoint)(void*);
  void*        arguments;
} proc_info_t;

typedef proc_info_t* proc_info_p;

extern proc_info_p procs[PROC_MAX];
extern proc_info_p current;

void scheduler_init();
void proc_create(proc_info_p proc, const char* name, void (*entrypoint)(void *), void* arg);
void proc_execute(proc_info_p proc);
void proc_exit(int code);
void proc_switch(void* state);
void proc_create_user(proc_info_p proc, const char* name, const void* code_blob, size_t code_size, void* arg);

#ifdef __cplusplus
}
#endif

#endif
