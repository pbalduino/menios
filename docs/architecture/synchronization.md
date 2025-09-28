# Synchronization Primitives

meniOS provides several synchronization building blocks inside the kernel. They
cover both busy-waiting primitives that are useful in low level contexts as well
as blocking primitives that integrate with the scheduler.

## Spinlocks

Spinlocks are used in short critical sections where sleeping is not permitted,
for instance while manipulating interrupt controller state or per-CPU data. The
implementation lives in `include/kernel/spinlock.h` and simply loops with pause
instructions until the lock becomes available.

## Mutexes

`kmutex_t` offers a blocking mutual exclusion primitive. If the lock is not
available, `kmutex_lock()` parks the current process and the scheduler will wake
it up when the mutex is released. This keeps the system from busy looping while
waiting for longer operations to finish.

## Condition Variables

Condition variables (`kcondvar_t`) allow threads to wait until another party
signals that a predicate changed. Callers hold an associated mutex, drop it
inside `kcondvar_wait()`, and resume once `kcondvar_signal()` or
`kcondvar_broadcast()` fires.

## Semaphores

`ksem_t` is a counting semaphore built on top of a mutex and condition
variable. Key operations include:

- `ksem_init()` – set the initial counter value.
- `ksem_wait()` – decrement the counter, sleeping while it is zero.
- `ksem_trywait()` – attempt to decrement without blocking.
- `ksem_post()` – increment the counter and wake a waiter.

Semaphores make it easy to model producer/consumer queues and fixed pools of
resources without reinventing the blocking logic each time.

## Read-Write Locks

`krwlock_t` provides shared (reader) and exclusive (writer) access guards. The
implementation prioritises writers to avoid starving threads waiting for
exclusive access.

- `krwlock_rdlock()` / `krwlock_rdunlock()` – acquire and release shared access;
  multiple readers may hold the lock simultaneously.
- `krwlock_wrlock()` / `krwlock_wrunlock()` – acquire and release exclusive
  access; writers wait until all readers exit and no writer is active.
- `krwlock_tryrdlock()` and `krwlock_trywrlock()` – non-blocking attempts that
  report whether the lock could be acquired immediately.

Read-write locks are a good fit for data structures that see frequent reads and
infrequent writes, minimising contention while still permitting safe
modification.
