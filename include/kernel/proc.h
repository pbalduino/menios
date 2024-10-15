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
#define PROC_STATE_TERMINATED 7

#define PROC_PRIO_NORMAL 3

#define PROC_PARENT_NONE 0

#define RLIMIT_DATA (4 * 1024 * 1024)

#define PROC_STACK_SIZE 1024

typedef struct cpu_state_t {
  uint64_t rax;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t rbp;
  uint64_t rsp;
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
  // phys_addr_t cr3;
  uint64_t rip;   
  uint64_t cs;    
  uint64_t rflags;
  uint64_t ss;    
} __attribute__((packed)) cpu_state_t;

typedef cpu_state_t* cpu_state_p;

typedef uint8_t proc_state_t;

typedef struct proc_info_t proc_info_t;
typedef proc_info_t* proc_info_p;

typedef struct proc_info_t {
  proc_info_p  parent;
  proc_info_p* children;
  uint32_t     children_count;
  uint32_t     pid;
  uintptr_t    brk;
  uintptr_t    heap;
  uintptr_t*   stack;
  proc_state_t state;
  uint8_t      priority;
  cpu_state_t* cpu_state;
  void(*entrypoint)(void*);
  void*        arguments;
  proc_info_p  next;
  char         name[32];
} proc_info_t;

typedef proc_info_t* proc_info_p;

extern proc_info_p procs[PROC_MAX];
extern proc_info_p current;

void init_scheduler();
void proc_create(proc_info_p proc, const char* name, void (*entrypoint)(void *), void* arg);
void proc_execute(proc_info_p proc);

#ifdef __cplusplus
}
#endif

#endif