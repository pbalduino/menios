# Road to Symmetric Multiprocessing (SMP)

This roadmap captures the scope, sequencing, and risks for turning Menios from a single-core environment into a kernel that can schedule across multiple processors. The plan assumes x86_64 hardware with APIC support and emphasises incremental bring-up to control complexity.

## Goals & Non-Goals

- Detect and initialise all CPUs exposed by ACPI/MP tables.
- Provide per-cpu data structures, schedulers, and interrupt routing that scale with core count.
- Make kernel subsystems (memory, scheduler, device drivers) safe under concurrent execution.
- Expose userland tooling to observe CPU topology and scheduling state.

Out of scope for initial delivery: NUMA-aware memory placement, CPU hotplug, heterogeneous cores, or hyper-threading optimisations beyond basic enablement. We also defer advanced performance tooling (perf events, tracing buffers) until the SMP base is stable.

## Current Baseline

- Boot code likely brings up only the bootstrap processor (BSP); AP startup path is absent.
- Global kernel structures (run queue, slab allocators, device registries) assume single-threaded access.
- Interrupt handling routes through legacy PIC/IOAPIC configuration tailored to one CPU.
- Timing sources (PIT/HPET/APIC timer) are calibrated for a single core.
- Testing and QEMU launch scripts target uniprocessor setups.

## Phase Breakdown

### Phase 1: Topology Discovery & Bootstrapping

- Parse ACPI MADT or legacy MP tables to enumerate CPU APIC IDs.
- Extend early boot code to prepare trampoline pages, per-cpu stacks, and GDT/IDT entries.
- Implement AP startup flow: INIT-SIPI-SIPI handshake, rendezvous barrier, feature checks.
- Provide per-cpu storage primitives (`get_cpu_var`, TLS-like data).

**Deliverables**
- Boot logs confirming all cores reach idle loop.
- Documentation describing CPU discovery and startup sequence.
- Developer toggles to limit core count for debugging.

**Risks / Open Questions**
- Need to confirm cross-toolchain support for 16-bit/real-mode trampoline build.
- Synchronising AP startup without deadlocks in early init code.

### Phase 2: Synchronisation Primitives

- Audit existing spinlocks/mutexes; introduce SMP-safe primitives with memory barriers.
- Define lock ordering and debugging aids (lockdep-lite, asserts).
- Ensure interrupt disable/enable semantics interact correctly with locks on every core.
- Convert global structures (scheduler queues, device tables) to use proper locking.

**Deliverables**
- Kernel-wide locking API documented in `include/` headers.
- Updated subsystems using the new primitives.
- Debug build checks to catch misuse.

**Risks / Open Questions**
- Risk of priority inversion or deadlock if lock hierarchies are unclear.
- Balancing lock granularity vs. complexity for early iterations.

### Phase 3: Scheduler & Run Queue Redesign

- Move from single run queue to per-cpu queues or a hybrid design with load balancer.
- Provide per-cpu idle threads and context-switch paths.
- Add load balancing heuristics (work stealing, periodic rebalance IPIs).
- Introduce CPU affinity handling in task structures and syscalls (optional early on).

**Deliverables**
- Scheduler capable of dispatching tasks across N cores.
- Instrumentation to observe run queue depth per CPU (debug console or trace buffer).
- Updated documentation and diagrams in `docs/architecture/`.

**Risks / Open Questions**
- Ensuring fairness and preventing thrashing on I/O-bound workloads.
- Need to re-evaluate scheduler assumptions around timer tick frequency on each core.

### Phase 4: Memory & TLB Coherence

- Make physical/virtual memory allocators thread-safe.
- Add TLB shootdown mechanism: per-cpu interrupt to invalidate entries during page table updates.
- Audit page fault handling, copy-on-write, and kmalloc caches for concurrency.
- Evaluate per-cpu caches/slabs to minimise contention.

**Deliverables**
- Working inter-processor interrupt (IPI) path for TLB invalidations.
- Stress tests for allocator correctness under multi-core load.
- Comments and docs explaining TLB shootdown strategy.

**Risks / Open Questions**
- Shootdown performance may regress; need metrics or heuristics.
- Page allocator might require refactor if it assumes single access path.

### Phase 5: Interrupts & Timers

- Transition from legacy PIC to APIC/LAPIC configuration if not already.
- Distribute device interrupts across cores; provide affinity controls.
- Calibrate local APIC timers per CPU and ensure scheduler ticks fire correctly.
- Implement reschedule and generic IPIs for cross-core coordination.

**Deliverables**
- Verified interrupt routing tables (debug output).
- Timer infrastructure that maintains consistent jiffies/timekeeping.
- Tools or docs for tuning IRQ affinity.

**Risks / Open Questions**
- IOAPIC programming varies by platform; need abstraction layer.
- Timer drift between cores causing scheduler inconsistencies.

### Phase 6: Driver & Subsystem Audit

- Review device drivers, filesystem code, and networking (if any) for SMP assumptions.
- Convert ad-hoc global state to per-cpu or locked variants.
- Provide guidelines for future driver authors on SMP-safe patterns.

**Deliverables**
- Audit report with resolved issues or tracked follow-ups.
- Updated coding guidelines referencing SMP requirements.
- Optional host tests targeting driver concurrency paths.

**Risks / Open Questions**
- Legacy drivers may require significant rewrites; prioritisation needed.
- Lack of hardware-in-the-loop tests complicates validation.

### Phase 7: Testing, Tooling, and Observability

- Extend `make run` configurations to launch multi-core QEMU/KVM sessions.
- Add stress tests (lock contention, memory alloc/free storms, scheduler shuffles).
- Integrate tracing or logging hooks for core migrations, lock contention, and IPIs.
- Update CI to run SMP smoke tests (at least dual-core).

**Deliverables**
- Automated test scenarios exercising multi-core code paths.
- Troubleshooting guide detailing common SMP bugs (deadlock, livelock, inconsistent state).
- Metrics collection or profiling hooks (optional but valuable).

**Risks / Open Questions**
- Longer test runtimes; may need tiered CI approach.
- Debugging SMP bugs without advanced tooling can stall progress—consider lightweight tracing early.

## Testing Strategy

- Layered approach: boot smoke tests (ensure APs online), kernel unit tests (locking, allocators), integration tests (scheduler fairness), and soak tests (long-running stress on multiple cores).
- Use QEMU with `-smp` to emulate different core counts; fix deterministic seeds for reproducibility.
- Instrument kernel with trace buffers or serial logging for race detection in early bring-up.
- Evaluate static analysis (`make check`) to flag new concurrency issues and run sanitiser builds if toolchain allows.

## Documentation & Developer Enablement

- Update architecture diagrams to reflect per-cpu structures and scheduler changes.
- Produce developer guides covering SMP-safe coding practices and common pitfalls.
- Document how to configure development environments (QEMU flags, hardware requirements).
- Reflect roadmap milestones in `MILESTONES.md` and track tasks in `tasks.json`.

## Suggested Milestone Order

1. Topology discovery and AP bootstrap (Phases 1-2, basic locks).
2. Scheduler and memory coherence (Phases 3-4).
3. Interrupt routing and subsystem audits (Phases 5-6).
4. Testing/observability and performance tuning (Phase 7).

Each milestone should conclude with stabilisation, regression testing, and documentation before progressing.

## Open Decisions

- Preferred lock primitive implementations (ticket locks vs. MCS) for scalability.
- Whether to retain legacy PIC paths for fallback hardware.
- Policy for CPU affinity and user exposure (syscalls, procfs equivalent).
- Extent of support for SMT/hyper-threading in initial release.

## Immediate Next Steps

1. Survey existing boot and interrupt code (`src/boot`, `src/arch/x86`) to confirm assumptions.
2. Draft an RFC outlining AP bring-up flow and required assembly additions.
3. Build a spike branch enabling dual-core boot with minimal locking to validate infrastructure.
4. Plan stress test harnesses and logging needed for early debugging.

Collecting answers to the open questions before implementation will keep the SMP effort focused and reduce rework.
