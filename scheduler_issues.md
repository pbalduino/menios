# Scheduler Reliability & Performance Issues

## 1. Ready Queue Updates Are Not Atomic
- **File**: `src/kernel/proc/proc.c`
- **Relevant Lines**: `ready_queue_push` (line 216) and calls from `proc_execute` (line 1634)
- **Bug**: `ready_queue_push` assumes exclusive access to `ready_queues`, but the caller `proc_execute` enqueues new processes with interrupts still enabled. A timer interrupt can invoke `proc_switch` while the list is mid-update, letting the scheduler traverse partially linked nodes and corrupt the queue.
- **Fix Idea**: Disable interrupts (or introduce a scheduler lock) around the enqueue, mirroring the guarded path in `proc_mark_ready`.

## 2. PID Table Never Populated
- **File**: `src/kernel/proc/proc.c`
- **Relevant Lines**: `procs[]` declaration (line 42), `proc_find_by_pid` (line 33), `proc_kill_pid` (line 1710)
- **Bug**: Newly created processes are not inserted into `procs[]`. Lookup routines (wait/kill/list) therefore fail to find any task except the kernel, breaking PID-based management and `/proc` style syscalls.
- **Fix Idea**: Assign each allocated process to a free slot in `procs[]` during creation/exec and clear it on teardown.

## 3. Process Teardown Leaks Resources
- **File**: `src/kernel/proc/proc.c`
- **Relevant Lines**: `scheduler_cleanup_process` (line 323) vs. `proc_free_resources` (line 188)
- **Bug**: When the scheduler reaps a terminated task it frees stacks and segments but skips `proc_file_table_cleanup` and `proc_release_user_memory`. File descriptors stay referenced and VM regions remain registered, leaking handles and physical pages.
- **Fix Idea**: Reuse `proc_free_resources` in `scheduler_cleanup_process`, or at least call the missing cleanup helpers before freeing the struct.
