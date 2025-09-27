# Atomic Operations & Memory Barrier Plan

Issue #41 tracks the introduction of kernel-wide atomic primitives and memory ordering helpers. This note captures the current landscape and the steps required to land the feature.

## Current State

* The kernel now provides `include/kernel/atomic.h`, a header-only wrapper over GCC/Clang `__atomic_*` builtins that exposes 32/64-bit load/store, exchange, compare-and-swap, and fetch-* helpers plus fence functions (`memory_barrier`, `smp_mb`, etc.).
* `include/kernel/spinlock.h` builds on the atomic API to offer spinlocks (with optional IRQ-save helpers) for short critical sections; `test/test_spinlock.c` covers basic lock/unlock semantics. Core subsystems (serial, console, kmalloc, ACPI glue) now use these spinlocks instead of ad-hoc kmutexes for brief critical sections.
* `test/test_atomic.c` exercises the raw atomic helpers in userland, validating CAS, fetch-add, and basic 64-bit operations via the host compiler.
* uACPI keeps using its private atomics; long term we may consolidate.

## Goals

1. Provide a minimal, well-defined atomic API for kernel developers with 32-bit and 64-bit integer support plus pointer operations.
2. Expose explicit memory order semantics (relaxed, acquire, release, acquire-release, sequentially consistent) without forcing consumers to deal with compiler-specific builtins directly.
3. Offer barrier helpers (`memory_barrier`, `read_barrier`, `write_barrier`, SMP variants) for use in lock implementations and critical sections.
4. Ensure the implementation works with GCC/Clang (our build toolchain) and leaves room for other architectures in the future.
5. Integrate the API with upcoming synchronisation primitives (spinlocks, RW locks, atomics-based wait queues) and SMP enablement work.

## Proposed API Surface

Header: `include/kernel/atomic.h`

```c
typedef struct {
    volatile uint32_t value;
} atomic32_t;

typedef struct {
    volatile uint64_t value;
} atomic64_t;

typedef enum {
    memory_order_relaxed,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
} memory_order_t;

uint32_t atomic_load32(const atomic32_t* obj, memory_order_t order);
void atomic_store32(atomic32_t* obj, uint32_t val, memory_order_t order);
uint32_t atomic_exchange32(atomic32_t* obj, uint32_t val, memory_order_t order);
bool atomic_compare_exchange32(atomic32_t* obj, uint32_t* expected, uint32_t desired, memory_order_t succ, memory_order_t fail);
uint32_t atomic_fetch_add32(atomic32_t* obj, uint32_t arg, memory_order_t order);
// mirror 64-bit versions and pointer aliases

void memory_barrier(void);
void read_barrier(void);
void write_barrier(void);
void smp_mb(void);
void smp_rmb(void);
void smp_wmb(void);
```

Implementation notes:

* Functions are inline wrappers around the `__atomic_*` builtins so call-sites stay clean and the compiler emits the correct instructions.
* Memory orders map directly to builtin order constants through `atomic_order_to_builtin()`.
* Pointer atomics can use the 64-bit variants on x86_64 until we add explicit typedefs.

## Integration Plan

1. **Adopt in Synchronisation Primitives**
   * Rework higher-level synchronisation (RW locks, mutexes) to call the spinlock/atomic helpers.
   * Replace remaining ad-hoc busy-wait loops and manual `lock xchg` sequences with `atomic_exchange`/`compare_exchange`.

2. **SMP & Kernel Usage**
   * Use the barrier wrappers (`smp_mb`/`smp_rmb`/`smp_wmb`) in scheduler, IPC, and future per-CPU structures once SMP lands.

3. **Testing**
   * Extend unit tests to cover failure paths (CAS failing) and ensure fences compile on all platforms we support.
   * When SMP support is in place, add runtime stress tests to validate memory ordering.

4. **Documentation & Examples**
   * Add cookbook examples (CAS loops, reference counting) once the primitives are used in-tree.

## Open Questions

* Should we expose 8/16-bit atomics? Current requirements focus on counters and flags, which can be satisfied with 32/64-bit operations.
* Do we need ticket locks or MCS locks out of the gate? That decision will follow once the base atomic API exists.
* For other architectures, we will need per-arch implementations. Keeping the API header-only and relying on compiler builtins should minimise the porting effort.

_Last updated: 2025-09-26_
