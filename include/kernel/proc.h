#ifndef MENIOS_INCLUDE_KERNEL_PROC_H
#define MENIOS_INCLUDE_KERNEL_PROC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#define PROC_KERNEL 0
#define PROC_MAX   16

#define PROC_STATE_IDLE    0
#define PROC_STATE_RUNNING 1

#define PROC_PRIO_NORMAL 3

#define PROC_PARENT_NONE 0

#define RLIMIT_DATA (4 * 1024 * 1024)

typedef struct {
    uintptr_t brk;
    uintptr_t heap;
    char*     name;
    int       parent_pid;
    int       pid;
    uint8_t   priority;
    uint8_t   state;
} proc_info_t;

typedef proc_info_t* proc_info_p;

extern proc_info_p procs[PROC_MAX];
extern proc_info_p current;

#ifdef __cplusplus
}
#endif

#endif