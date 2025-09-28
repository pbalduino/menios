# Scheduler Architecture

The meniOS scheduler now preempts userland workloads using a multi-level
priority queue. Each runnable process carries a fixed quantum expressed in
microseconds; the timer interrupt decrements the current process' slice and
dispatches the next runnable task when the slice expires, the task blocks, or a
higher-priority task becomes ready.

## Run Queues & Sleep List

* Ready processes live in per-priority FIFO queues (idle → realtime). The
  scheduler always selects from the highest non-empty queue, providing
  priority-aware round-robin scheduling.
* Sleeping processes are kept in a time-ordered list keyed by wake-up deadline.
  On every tick, expired sleepers are moved back to their respective ready
  queues.
* Terminated processes are drained lazily when encountered, which triggers the
  existing stack and address-space teardown path.

## Time Slicing

* Default quanta: idle (1 ms), low (4 ms), normal (6 ms), high (8 ms), realtime
  (12 ms). `scheduler_set_quantum()` allows tuning per priority.
* Each process tracks `quantum_us`, `time_slice_remaining_us`, and
  `last_dispatch_us` for accounting. CPU time is accumulated in
  `exec_time`, while `dispatch_count` records how many times the process has run.

## Priorities & Policies

* `proc_set_priority()` clamps and applies new priorities dynamically.
* Preemption occurs immediately if a higher-priority task enters the ready
  queues, ensuring latency-sensitive workloads (e.g., Doom) receive CPU quickly.

## Sleep & Yield Primitives

- New syscalls (`SYS_YIELD`, `SYS_SLEEP`) expose cooperative hooks for userland.
  `SYS_SLEEP` takes a duration in microseconds and parks the process until the
  deadline expires.
- Kernel threads use the same helpers; `ksleep()` now blocks via the scheduler
  rather than spinning on the TSC.
- Blocking mutexes and condition variables are wired into the scheduler via
  `proc_mark_ready`, so waiters sleep in the normal queues instead of busy
  looping on spinlocks.

## Kernel Interaction

* The kernel process acts as the idle task with the lowest priority and is never
  enqueued; it simply runs when no other ready task exists.
* Kernel-managed threads inherit normal priority by default but can be boosted
  if necessary.
* `kthread_join()` now blocks on a condition variable rather than polling,
  allowing callers to sleep while the worker thread runs and wake immediately
  on completion.

## Statistics & Future Work

* Per-process accounting captures cumulative CPU time and dispatch counts. These
  stats will feed future observability (e.g., `/proc`-style introspection).
* Follow-up items include exposing configuration knobs to userspace, integrating
  with blocking primitives (mutexes/condition variables), and introducing
  deadline-aware policies.

## Demo Harness

The `user_demo_launch()` helper now spawns three user processes (low, normal,
and high priority) that repeatedly write to stdout, sleep for 200 ms, and issue
explicit yields. This provides an out-of-the-box way to observe preemption,
sleep/wake transitions, and priority ordering on the serial console.
