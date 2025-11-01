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
5. Capture the updated UACPI capabilities in design docs and confirm SCI routing with the new IRQ slot map.
6. Prioritise driver bring-up (#67 e1000, #382/#383 audio) now that the infrastructure is in place.

Collecting answers to the open questions before implementation will keep the SMP effort focused and reduce rework.

## Related Issues & Current Work

### Completed Foundation
- ✅ **#93** - Fork/exec process creation (scheduler foundation)
- ✅ **#57** - Virtual memory manager (memory infrastructure)
- ✅ **Limine bootloader** - Provides SMP boot support
- ✅ **APIC/LAPIC** - Basic interrupt routing infrastructure

### ACPI/Hardware Prerequisites (Completed)
- ✅ **#276** - UACPI integration (22/22 primitives)
  - MADT parsing, PM timer, firmware requests, SCI IRQ path
- ✅ **#278** - Work queue infrastructure (deferred GPE/Notify support)
- ✅ **#279** - Dynamic IRQ allocation (slot map + user-mode aware IRQ teardown)
- ✅ **#281** - I/O port management (safe reservation API)
- ✅ **#280** - PCI configuration space access (MMCONFIG support)
- ✅ **#335** - PCI enumeration/access (device discovery ready)

### Planned SMP Work (Sequential Implementation)
- 📋 **#80** - Implement per-CPU data infrastructure
  - `get_cpu_var()` primitives
  - TLS-like per-CPU storage
  - CPU ID accessors

- 📋 **#81** - Implement APIC/MADT CPU enumeration
  - Parse MADT table for CPU APIC IDs
  - Discover available cores
  - Map LAPIC registers

- 📋 **#82** - Implement AP startup and BSP → AP bring-up
  - INIT-SIPI-SIPI sequence
  - Trampoline code (16-bit → 64-bit transition)
  - Per-CPU stack and GDT setup
  - Rendezvous barrier

- 📋 **#85** - Audit kernel structures for SMP safety
  - Convert global data to per-CPU or locked
  - Add spinlocks/mutexes with proper ordering
  - Memory barriers and atomic operations
  - Document lock hierarchies

- 📋 **#83** - Extend scheduler for SMP with per-CPU runqueues
  - Per-CPU run queues
  - Load balancing (work stealing)
  - CPU affinity support
  - IPI-based preemption

- 📋 **#84** - Implement interrupt distribution and IPIs
  - Distribute device IRQs across cores
  - Inter-processor interrupts (IPI)
  - TLB shootdown IPIs
  - Reschedule IPIs
  - Per-CPU APIC timer setup

- 📋 **#86** - Add SMP testing and diagnostics
  - Multi-core QEMU configurations
  - Stress tests (lock contention, scheduler fairness)
  - Diagnostic tools (CPU topology, run queue state)
  - Tracing/logging hooks

### Implementation Order (Critical Path)

```
Phase 1: Foundation
├─ Complete #276 (UACPI)
├─ Complete #278, #279, #281 (UACPI blockers)
├─ #80 (per-CPU data)
└─ #81 (CPU enumeration)

Phase 2: Bring-Up
├─ #82 (AP startup)
└─ #85 (SMP safety audit)

Phase 3: Scheduler & Interrupts
├─ #83 (per-CPU scheduler)
└─ #84 (IRQ distribution + IPIs)

Phase 4: Validation
└─ #86 (testing & diagnostics)
```

### Key Dependencies

**Before Phase 1:**
1. ⏳ Complete #276 (UACPI integration to 100%)
2. ⏳ Complete #279 (dynamic IRQ - critical for interrupt routing)
3. ⏳ Complete #278, #281 (work queue, I/O port mgmt)

**Before Phase 2:**
1. ✅ Complete Phase 1 (#80, #81)
2. 📝 Document current scheduler and memory manager assumptions
3. 📝 Design per-CPU memory allocator strategy

**Before Phase 3:**
1. ✅ Complete Phase 2 (#82, #85)
2. 📝 Define lock ordering and primitives
3. 🧪 Validate dual-core boot in QEMU

**Before Phase 4:**
1. ✅ Complete Phase 3 (#83, #84)
2. 🧪 Basic multi-core functionality working
3. 📝 Performance baselines established

### Estimated Timeline

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| **Phase 1** | 4-6 weeks | UACPI completion, per-CPU infra |
| **Phase 2** | 4-6 weeks | AP bring-up, safety audit |
| **Phase 3** | 6-8 weeks | Scheduler redesign, IPI impl |
| **Phase 4** | 2-4 weeks | Testing, tuning, observability |
| **Total** | **4-6 months** | Sequential with some overlap |

### Current Blockers

**High Priority:**
- **#279** - Dynamic IRQ allocation (blocks interrupt distribution)
- **#276** - UACPI completion (blocks CPU enumeration)

**Medium Priority:**
- **#278** - Work queue (needed for UACPI GPE)
- **#281** - I/O port management (needed for UACPI)

**Unblocked:**
- **#80** - Per-CPU data (can start anytime)
- **#85** - SMP safety audit (can start anytime)

---

**Last Updated:** 2025-10-28
**Status:** Foundation in place (SMP-ready), awaiting UACPI completion to start Phase 1
**Timeline:** 4-6 months for full SMP support (after UACPI blockers resolved)
**Current Focus:** Complete #276 (UACPI) and #279 (dynamic IRQ) first
