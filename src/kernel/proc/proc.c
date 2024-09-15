#include <kernel/proc.h>
#include <types.h>

proc_info_t kernel_process_info = {
  .pid = 0,
  .state = PROC_STATE_RUNNING,
  .priority = PROC_PRIO_NORMAL,
  .parent_pid = PROC_PARENT_NONE,
  .name = "kernel"
};

proc_info_p procs[PROC_MAX] = {
  &kernel_process_info
};

proc_info_p current = &kernel_process_info;