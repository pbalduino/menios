# meniOS Issue Dependency Analysis

This document provides a comprehensive analysis of dependencies between open issues in the meniOS project, helping prioritize development efforts.

## 🎯 Critical Path Issues

These issues form the backbone of the system and should be prioritized:

### Tier 1: Foundation
1. **#35** - kmalloc implementation (enables all kernel features)
2. **#57** - VM manager (vm_map/vm_unmap/vm_clone)
3. **#96** - File descriptor management
4. **#34** - Preemptive scheduler

### Tier 2: Core Systems
5. **#89** - mmap/munmap syscalls
6. **#93** - fork/exec process creation
7. **#36** - mutex implementation
8. **#101** - LAPIC/HPET timer integration

### Tier 3: IPC Foundation
9. **#102** - pipes (pipe/mkfifo)
10. **#40** - condition variables
11. **#103** - UNIX signals
12. **#104** - shared memory (shmget/shmat/shmdt)

## 📊 Dependency Categories

### Memory Management Chain
```
#35 (kmalloc) → #57 (VM manager) → #89 (mmap/munmap) → #95 (userspace malloc)
                                 ↓
                              #93 (fork/exec)
                                 ↓
                              #104 (shared memory)
```

### Synchronization Chain
```
#34 (scheduler) → #36 (mutex) → #37 (semaphore)
                              → #39 (rwlock)
                              → #40 (condition variables)
                                     ↓
                              #102/#104/#105 (IPC systems)
```

### IPC Communication Chain
```
#96 (file descriptors) → #102 (pipes) → #106 (microkernel IPC)
                      → #105 (Unix sockets)
                                             ↓
#101 (timers) ──────────────────────────→ #107 (capability security)
```

### Networking Stack
```
#67 (network driver) → #68 (Ethernet/ARP) → #69 (IPv4/ICMP) → #70 (UDP/TCP) → #71 (socket API)
                                                                                      ↓
                                                                              #105 (Unix sockets)
```

### Filesystem Stack
```
#62 (block driver) → #63 (block cache) → #64 (filesystem lib) → #65 (VFS) → #60 (syscalls)
                                                                              ↓
                                                                      #96 (file descriptors)
```

## 🏗️ Implementation Phases

### Phase 1: Core Foundation (Essential)
**Goal**: Basic kernel functionality
- #35: kmalloc implementation
- #57: VM manager (vm_map/vm_unmap)
- #34: Preemptive scheduler
- #36: mutex implementation

**Why First**: These provide the fundamental infrastructure every other feature depends on.

### Phase 2: Process Management (High Priority)
**Goal**: Process creation and basic IPC
- #96: File descriptor management
- #89: mmap/munmap syscalls
- #93: fork/exec process creation
- #101: LAPIC/HPET timers
- #102: pipes implementation

**Why Second**: Enables basic process management and simple IPC.

### Phase 3: Advanced IPC (Medium Priority)
**Goal**: Full IPC suite for applications
- #40: condition variables
- #103: UNIX signals
- #104: shared memory
- #105: Unix domain sockets
- #94: signal handling system

**Why Third**: Provides complete IPC functionality for complex applications.

### Phase 4: Microkernel Transition (Advanced)
**Goal**: Microkernel architecture
- #106: microkernel message passing IPC
- #107: capability-based security
- #97: architecture abstraction layer

**Why Fourth**: Advanced features for microkernel architecture.

### Phase 5: Specialized Systems (Optional/Parallel)
**Goal**: Complete system functionality
- **Networking**: #67→#68→#69→#70→#71→#72→#73
- **Filesystem**: #62→#63→#64→#65→#60
- **SMP**: #80→#81→#82→#83→#84
- **Advanced Memory**: #87→#88→#90→#91→#95
- **Hardware**: #31, #32, #33

## 🔴 Blocking Relationships

### Cannot Start Until Complete:
- **#93 (fork/exec)** blocks on: #57 (VM), #96 (file descriptors)
- **#106 (microkernel IPC)** blocks on: #101 (timers), #57 (VM), #40 (condition variables)
- **#107 (capability security)** blocks on: #106 (microkernel IPC)
- **#105 (Unix sockets)** blocks on: #71 (socket API), #96 (file descriptors)
- **#95 (userspace malloc)** blocks on: #89 (mmap/munmap)

### Parallel Development Opportunities:
- **Networking stack** (#67-#73) can develop independently after basic kernel
- **SMP support** (#80-#84) can develop in parallel with IPC
- **Filesystem** (#62-#65) can develop independently
- **jemalloc research** (#87-#92) can happen in parallel

## 🎯 Recommended Focus Areas

### For Maximum Impact:
1. **Start with #35, #57, #34** - Core foundation
2. **Then #96, #89, #93** - Process management
3. **Then #102, #103, #104** - Essential IPC

### For Running Applications (like Doom):
1. **Memory**: #35 → #57 → #89 → #95
2. **Processes**: #93 → #94
3. **I/O**: #96 → #60 (with #62→#65 filesystem chain)
4. **Graphics**: #31 (framebuffer interface)

### For Microkernel Vision:
1. **Foundation**: Complete Phases 1-3 first
2. **Transition**: #106 → #107
3. **Architecture**: #97 for multi-platform support

## 📈 Effort vs Impact Analysis

### High Impact, Low Effort:
- #35 (kmalloc) - Small but critical
- #36 (mutex) - Well-understood implementation
- #102 (pipes) - Straightforward after file descriptors

### High Impact, High Effort:
- #57 (VM manager) - Complex but foundational
- #93 (fork/exec) - Sophisticated but essential
- #106 (microkernel IPC) - Advanced but enables architecture

### Medium Impact, Variable Effort:
- **Networking stack** - High effort, medium priority for basic OS
- **SMP support** - High effort, nice-to-have for most use cases
- **Filesystem** - Medium effort, high user value

This analysis provides a roadmap for systematic development, ensuring that foundational issues are addressed before dependent features, maximizing development efficiency and system stability.