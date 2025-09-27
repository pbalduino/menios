# Atomic Operations & Memory Barrier Plan

Issue #41 tracks the introduction of kernel-wide atomic primitives and memory ordering helpers. This note captures the current landscape and the steps required to land the feature.

## Current State

* There is no in-tree atomic API for kernel code. Synchronisation uses coarse primitives (`kmutex`, future spinlocks) built directly on instruction sequences.
* Vendor code (uACPI) bundles its own atomic helpers under `include/uacpi/platform/atomic.h`, relying on compiler builtins where available. These are scoped to the ACPI component and not exposed to the rest of the kernel.
* No memory-barrier macros exist beyond implicit serialisation in `cli/sti` or lock-prefixed instructions.

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

Implementation strategy:

* Use GCC/Clang `__atomic_*` builtins to back the API. They already map to the correct instructions and honour the requested memory ordering.
* Wrap the builtins in inline functions to keep call-sites clean and allow future per-arch overrides.
* Provide convenience macros for pointer atomics (alias the 64-bit path on x86_64).

## Integration Plan

1. **Header & Inline Implementation**
   * Create `include/kernel/atomic.h` and `src/kernel/atomic/atomic.c` (or header-only inline implementations) wrapping the GCC builtins.
   * Provide compile-time assertions to ensure `sizeof(_Atomic)` assumptions hold on x86_64.

2. **Barriers**
   * Introduce barriers as thin wrappers over `__atomic_thread_fence()` using the appropriate orderings.
   * Optionally provide architecture-specific fallbacks using `asm volatile ("mfence" ::: "memory")` if the compiler builtins are unavailable.

3. **Adopt in Synchronisation Primitives**
   * Update pending spinlock/RW lock tasks to use the new API.
   * Replace ad-hoc busy-wait loops with `atomic_exchange`/`compare_exchange` once the primitives exist.

4. **Testing**
   * Add unit tests under `test/` that validate basic atomic behaviour (CAS success/failure, fetch-add semantics) using fake SMP scenarios.
   * For barriers, rely on compiler intrinsics; deeper validation will come alongside SMP work.

5. **Documentation & Examples**
   * Expand this note once prototypes land, adding usage examples for compare-and-swap loops and barrier placement guidelines.
   * Update the coding guidelines (if needed) to point contributors at the atomic API rather than rolling their own inline assembly.

## Open Questions

* Should we expose 8/16-bit atomics? Current requirements focus on counters and flags, which can be satisfied with 32/64-bit operations.
* Do we need ticket locks or MCS locks out of the gate? That decision will follow once the base atomic API exists.
* For other architectures, we will need per-arch implementations. Keeping the API header-only and relying on compiler builtins should minimise the porting effort.

_Last updated: 2025-09-26_
