# meniOS Issue Dependency Analysis

This document provides a comprehensive analysis of dependencies between open issues in the meniOS project, helping prioritize development efforts.

## 🎯 Critical Path Issues

These issues form the backbone of the system and should be prioritized:

### Tier 1: Foundation (COMPLETE!)
1. **#35** - kmalloc implementation (CLOSED - enables all kernel features)
2. **#57** - VM manager (CLOSED - vm_map/vm_unmap/vm_clone)
3. **#96** - File descriptor management (CLOSED)
4. **#34** - Preemptive scheduler (CLOSED)

### Tier 2: Core Systems (PARTIALLY COMPLETE)
5. **#89** - mmap/munmap syscalls (CLOSED)
6. **#93** - fork/exec process creation (CLOSED)
7. **#36** - mutex implementation (CLOSED)
8. **#101** - LAPIC/HPET timer integration (CLOSED)

### Tier 3: IPC Foundation (PARTIALLY COMPLETE)
9. **#102** - pipes (pipe/mkfifo)
10. **#40** - condition variables (CLOSED)
11. **#103** - UNIX signals (handler/mask pipeline implemented)
12. **#104** - shared memory (shmget/shmat/shmdt)

### Tier 4: Threading Support (PARTIALLY COMPLETE)
13. **#108** - Kernel threading infrastructure (CLOSED)
14. **#109** - pthread API and POSIX threading support
15. **#110** - Thread-safe C library (libc)
16. **#111** - Advanced pthread synchronization primitives
17. **#112** - Thread debugging and profiling support
18. **#113** - Thread-aware system calls and kernel integration

### 🆕 Tier 5: Unicode & Internationalization (NEW!)
19. **#127** - UTF-8 encoding/decoding utilities (foundation)
20. **#128** - Unicode font system (BDF conversion)
21. **#129** - Unicode text rendering in framebuffer
22. **#130** - Unicode keyboard input processing
23. **#131** - Unicode normalization and text processing
24. **#132** - Filesystem Unicode filename support
25. **#133** - Locale/i18n framework
26. **#134** - Unicode testing and validation

### 🆕 Tier 6: Testing & Quality Assurance (NEW!)
27. **#135** - Code coverage reporting with Gcov integration

### 🆕 Tier 7: Device Filesystem & /dev (NEW!)
28. **#136** - Device filesystem infrastructure (foundation)
29. **#137** - /dev/null and /dev/zero virtual devices
30. **#138** - /dev/console and terminal devices
31. **#139** - /dev/random and /dev/urandom entropy devices
32. **#140** - /dev/kbd0 and /dev/fb0 hardware devices
33. **#141** - /dev/sda block devices for storage
34. **#142** - /dev/ttyS0 serial device interface

### 🆕 Tier 8: Mouse Input Support (NEW!)
35. **#143** - PS/2 mouse driver and input support
36. **#144** - USB mouse support through HID class driver

### 🆕 Tier 9: Virtual & Special Filesystems (NEW!)
37. **#145** - tmpfs/ramfs (in-memory filesystem for /tmp)
38. **#146** - devfs (device filesystem for /dev hierarchy)
39. **#147** - procfs (process information filesystem for /proc)
40. **#148** - ext2 filesystem support (read-only initially)

### 🆕 Tier 10: init & Shell Infrastructure (NEW!)
41. **#149** - wait/waitpid syscall (process synchronization - CLOSED)
42. **#150** - Process zombie state and orphan reparenting (CLOSED)
43. **#151** - getcwd/chdir syscalls (directory navigation - CLOSED)
44. **#152** - Environment variables support (getenv/setenv/unsetenv - CLOSED)
45. **#153** - Barebone init program (PID 1 process supervisor - CLOSED)
46. **#154** - Boot integration to start init as PID 1 (CLOSED)

### Tier 11: mosh Shell Core Components
47. **#161** - Basic REPL and command parsing (read/parse/execute loop - CLOSED)
48. **#162** - Command execution (fork/exec/wait infrastructure - CLOSED)
49. **#163** - Built-in commands (cd/pwd/exit/export)
50. **#164** - Basic I/O redirection (>, <, >>)
51. **#165** - Pipe support (|)

### Tier 12: mosh Shell Advanced Features
52. **#156** - Command history (up/down arrows)
53. **#157** - Tab completion
54. **#160** - Line editing keys (Ctrl+L/K/U/A/E/R)
55. **#155** - Scripting support (if/while/for/functions)
56. **#158** - Job control (bg/fg/Ctrl-Z)
57. **#159** - Advanced redirection (2>&1, here-docs)

### Tier 13: Multi-User System Infrastructure (Future)
58. **#166** - User/group database infrastructure (/etc/passwd, /etc/group)
59. **#167** - Process credentials (UID/GID per process)
60. **#168** - User database parsing and management
61. **#169** - Login program and authentication
62. **#170** - Session management and getty
63. **#171** - File system permission bits (owner/group/other)
64. **#172** - VFS permission checking
65. **#173** - Security syscalls (getuid/setuid family)
66. **#174** - Update all syscalls for permission checks
67. **#175** - User management utilities (useradd, passwd, chmod)
68. **#176** - su and sudo implementation
69. **#177** - Resource limits (ulimit/getrlimit/setrlimit)
70. **#178** - Security testing and audit

**Note:** Multi-user support is LOW PRIORITY - implement after shell and core applications are working. See `docs/roads/road_to_multiuser.md` for full details.

## 📊 Updated Dependency Categories

### Memory Management Chain (CORE COMPLETE!)
```
#35 (kmalloc) → #57 (VM manager) → #89 (mmap/munmap) → #95 (userspace malloc)
                                        ↓
                                     #93 (fork/exec)
                                        ↓
                                     #104 (shared memory)
```

### Synchronization Chain (CORE COMPLETE!)
```
#34 (scheduler) → #36 (mutex) → #37 (semaphore)
                                → #39 (rwlock)
                                → #40 (condition variables)
                                        ↓
                                 #102/#104/#105 (IPC systems)
```

### Threading Chain (FOUNDATION COMPLETE!)
```
#34 (scheduler) ────┐
#36 (mutex) ────────┼──→ #108 (kernel threading - CLOSED) ──→ #109 (pthread API)
#57 (VM manager) ───┘                                                ↓
                                                             #110 (thread-safe libc)
                                                                     ↓
                       #88 (TLS) ──→ #109 ──→ #111 (advanced pthread sync)
                                                     ↓
                       #94 (signals) ──→ #109 ──→ #112 (debugging/profiling)
                                                     ↓
                       #108 ──→ #113 (thread-aware syscalls)
```

### IPC Communication Chain (FOUNDATION READY)
```
#96 (file descriptors - CLOSED) → #102 (pipes) → #106 (microkernel IPC)
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

### 🆕 Unicode Support Chain (NEW!)
```
Phase 1: Foundation
#127 (UTF-8 utilities) → #128 (Unicode fonts) → #129 (text rendering)
                      ↓                              ↓
Phase 2: Enhanced    #131 (normalization)    #130 (keyboard input)
                           ↓                        ↓
Phase 3: System      #132 (filesystem) → #133 (locale/i18n)
                           ↓                        ↓
Phase 4: Testing     #134 (comprehensive testing) ←──────────┘
```

### Hardware Driver Expansion
```
IDE Storage:
#114 (block abstraction) → #118 (IDE/PATA) → #119 (IDE init) → #120 (IDE interrupts)

USB Support:
#121 (USB host controller) → #122 (USB device mgmt) → #123 (USB transfers)
                                    ↓                       ↓
                              #125 (USB HID)           #124 (USB hub)
                                    ↓                       ↓
                              ✅ #32 (input)         #126 (USB storage)
                                                           ↓
                                                    #63 (block cache)
```

### 🆕 Testing & Quality Assurance Chain (NEW!)
```
Unity Test Framework (existing) → #134 (Unicode testing) → #135 (code coverage)
                                                                   ↓
                                                           Coverage feedback loop
                                                           (improves all components)
```

### 🆕 Device Filesystem Chain (NEW!)
```
Phase 1: Foundation
#96 (file descriptors) ──┐
#60 (VFS layer) ─────────┼──→ #136 (device filesystem infrastructure)
                         │            ↓
Phase 2: Virtual Devices                #137 (null/zero) ──┐
                         │            ↓                    │
Phase 3: Hardware        │      #138 (console/tty) ──┐    │
#31 (framebuffer - CLOSED) ──┼──→   #140 (kbd/fb devices) │    │
#32 (keyboard - CLOSED) ─────┘      #141 (block devices) ──┼───┤
#62 (AHCI - CLOSED) ─────────────→   #142 (serial device) ──┘   │
                         │                                  │
Phase 4: Advanced                #139 (random devices) ─────┤
                                                            │
Phase 5: Complete /dev System ←─────────────────────────────┘
```

### 🆕 Mouse Input Chain (NEW!)
```
Phase 1: PS/2 Support
#22 (hardware probing) → #143 (PS/2 mouse driver) → #32 (input subsystem)
                                                           ↓
Phase 2: USB Support                                  Mouse events
#125 (USB HID) ──────────→ #144 (USB mouse) ──────────────┘
```

### 🆕 Virtual Filesystems Chain (NEW!)
```
Phase 1: Foundation (VFS infrastructure ready)
#65 (VFS - CLOSED) ──┐
#96 (file descriptors - CLOSED) ──┐
                                  ↓
Phase 2: In-Memory FS        #145 (tmpfs/ramfs) → /tmp for temporary files
                                  ↓
Phase 3: Device FS           #146 (devfs) → Integrates with #136-#142 (/dev hierarchy)
                                  ↓         Exposes devices as files
                                  ↓
Phase 4: Process Info        #147 (procfs) → /proc for system introspection
                                  ↓         Process monitoring/debugging
                                  ↓
Phase 5: Block FS            #148 (ext2) → Better persistent storage
#62 (block driver) ──────────────┘         Alternative to FAT32
#63 (block cache) ───────────────┘         Native Linux filesystem
```

### 🆕 init & Shell Chain (NEW!)
```
Phase 1: Process Synchronization
#93 (fork/exec - CLOSED) ──┐
#60 (syscalls - CLOSED) ────┼──→ #149 (wait/waitpid - CLOSED) → Process can wait for children
                            │            ↓
Phase 2: Zombie Handling    │      #150 (zombie state + orphan reparenting - CLOSED)
                            │            ↓                Parent reaps children
                            │            ↓                Orphans go to init
Phase 3: Shell Support      │            ↓
#65 (VFS - CLOSED) ─────────┼──→   #151 (getcwd/chdir - CLOSED) → Directory navigation
                            │            ↓
                            └──→   #152 (environment vars - CLOSED) → PATH, HOME, etc.
                                         ↓
Phase 4: init Program                    ├──→ #153 (init program - CLOSED)
#149 (wait/waitpid - CLOSED) ─────────────────────┘         ↓  Reaps zombies
#150 (zombie handling - CLOSED) ────────────────────────┘  Supervises processes
                                                    ↓
Phase 5: Boot Integration                    #154 (boot init as PID 1 - CLOSED)
                                                    ↓  Start init at boot (DONE)
                                                    ↓
```

### mosh Shell Component Chain
```
Phase 1: Core Infrastructure (Tier 11 - Critical)
#149 (wait - CLOSED) ──┐
#151 (getcwd/chdir - CLOSED) ────┼──→ #161 (REPL & parsing - CLOSED) → Read, parse, tokenize commands
#152 (environment - CLOSED) ─────┘          ↓
                                   ↓
                               #162 (command execution - CLOSED) → fork/exec/wait/PATH lookup
                                   ↓
                              #163 (built-ins) → cd/pwd/exit/export

Phase 2: I/O & Pipes (Tier 11 - High Priority)
#96 (file descriptors - CLOSED) → #164 (basic redirection) → >, <, >>
#102 (pipes) ────────────────────→ #165 (pipe support) → cmd1 | cmd2 | cmd3

Phase 3: Usability Features (Tier 12 - Quality of Life)
Terminal support ─────→ #156 (command history) → Up/down arrows, Ctrl+R
                    ├─→ #157 (tab completion) → Complete commands/files
                    └─→ #160 (line editing) → Ctrl+L/K/U/A/E/R

Phase 4: Advanced Features (Tier 12 - Optional)
#155 (scripting) → if/while/for/functions
#158 (job control) → bg/fg/Ctrl-Z/process groups
#159 (advanced redirection) → 2>&1, here-docs, <<EOF
```

### Filesystem Stack (COMPLETE!)
```
#62 (block driver - CLOSED) → #63 (block cache - CLOSED) → #64 (filesystem lib - CLOSED) → #65 (VFS - CLOSED) → #60 (syscalls - CLOSED)
                                                                                                                                ↓
                                                                                                                      #96 (file descriptors - CLOSED)
```

### Multi-User System Chain (Tier 13 - Future)
```
Phase 1: User Infrastructure
#60 (file I/O - CLOSED) ──┐
                          ├──→ #166 (user/group database) → /etc/passwd, /etc/group, /etc/shadow
                          │          ↓
#93 (fork/exec - CLOSED) ─┼──→ #167 (process credentials) → UID/GID per process
                          │          ↓
                          └──→ #168 (database parsing) → Read user/group data

Phase 2: Authentication
#167 (credentials) ───────→ #169 (login program) → Username/password auth
                                    ↓
                               #170 (session management) → Getty, TTY allocation

Phase 3: File Permissions
#65 (VFS - CLOSED) ───────→ #171 (permission bits) → Owner/group/other rwx
                                    ↓
#167 (credentials) ───────→ #172 (permission checking) → Enforce access control

Phase 4: Security Syscalls
#167 (credentials) ───────→ #173 (security syscalls) → getuid/setuid family
                                    ↓
                               #174 (syscall updates) → Permission checks everywhere

Phase 5: User Tools
#168 (database) ──────────→ #175 (user utilities) → useradd, passwd, chmod
#173 (security) ──────────→ #176 (su/sudo) → Privilege elevation

Phase 6: Resource Control
#167 (credentials) ───────→ #177 (resource limits) → ulimit, quotas
                                    ↓
All phases ───────────────→ #178 (security audit) → Testing and hardening
```

## 🏗️ Updated Implementation Phases

### Phase 1: Core Foundation (COMPLETE!)
**Goal**: Basic kernel functionality
- #35: kmalloc implementation (CLOSED)
- #57: VM manager (vm_map/vm_unmap) (CLOSED)
- #34: Preemptive scheduler (CLOSED)
- #36: mutex implementation (CLOSED)

**Status**: Foundation is solid! All core systems operational.

### Phase 2: Process & I/O Management (MOSTLY COMPLETE!)
**Goal**: Process creation and basic IPC
- #96: File descriptor management (CLOSED)
- #89: mmap/munmap syscalls (CLOSED)
- #93: fork/exec process creation (CLOSED)
- #101: LAPIC/HPET timers (CLOSED)
- #102: pipes implementation

**Status**: Core process management operational, IPC components remaining.

### Phase 3: Threading Support (IN PROGRESS)
**Goal**: Full multithreading capability
- #108: Kernel threading infrastructure (CLOSED)
- #109: pthread API and POSIX threading
- #113: Thread-aware system calls
- #110: Thread-safe C library
- #111: Advanced pthread synchronization
- #112: Thread debugging and profiling

**Status**: Foundation complete, userland threading APIs ready to implement.

### Phase 4: Advanced IPC (READY TO START)
**Goal**: Full IPC suite for applications
- #40: condition variables (CLOSED)
- #103: UNIX signals (handlers + sigaction/sigprocmask complete)
- #104: shared memory
- #105: Unix domain sockets
- #94: signal handling system

**Status**: Prerequisites met, ready for implementation.

### Phase 5: Microkernel Transition (Advanced)
**Goal**: Microkernel architecture
- #106: microkernel message passing IPC
- #107: capability-based security
- #97: architecture abstraction layer

**Why Fifth**: Advanced features for microkernel architecture.

### Phase 6: Specialized Systems (PARALLEL DEVELOPMENT)
**Goal**: Complete system functionality
- **Networking**: #67→#68→#69→#70→#71→#72→#73
- **Filesystem**: #62→#63→#64→#65→#60 (COMPLETE!)
- **SMP**: #80→#81→#82→#83→#84
- **Advanced Memory**: #87→#88→#90→#91→#95
- **Hardware**: #31 (CLOSED), #32 (CLOSED), #33

## 🔴 Current Blocking Relationships

### Ready to Start (Dependencies Met):
- **#151 (getcwd/chdir - CLOSED)** - Directory navigation delivered
- **#152 (environment vars - CLOSED)** - PATH/HOME plumbing delivered
- **#109 (pthread API)** - Dependencies met: #108 (CLOSED)
- **#110 (thread-safe libc)** - Can start in parallel with #109
- **#127 (UTF-8 utilities)** - No dependencies, ready to start immediately!
- **#135 (code coverage)** - Can start with existing Unity tests, no blocking dependencies
- **#136 (device filesystem)** - Dependencies: #96 (CLOSED), #60 (CLOSED) - ready!
- **#145 (tmpfs/ramfs)** - Dependencies: #65 (VFS - CLOSED), #96 (CLOSED) - ready!
- **#146 (devfs)** - Dependencies: #65 (VFS - CLOSED), #96 (CLOSED) - ready!
- **#147 (procfs)** - Dependencies: #65 (VFS - CLOSED), process management (CLOSED) - ready!
- **#148 (ext2)** - Dependencies: #62-#65 (storage stack - CLOSED) - ready!
- **#102 (pipes)** - Basic IPC implementation
- **#103 (UNIX signals)** - Process control mechanism (handlers delivered; siginfo/rt signals TBD)

### Cannot Start Until Complete:
- **Shell Core Components:**
  - **#162 (command execution)** blocks on: #161 (REPL/parsing)
  - **#163 (built-ins)** blocks on: #161 (REPL/parsing)
  - **#164 (basic redirection)** blocks on: #161 (REPL/parsing)
  - **#165 (pipe support)** blocks on: #102 (pipes syscall), #161 (REPL/parsing)
- **Shell Advanced Features:**
  - **#156 (command history)** blocks on: #161 (REPL/parsing)
  - **#157 (tab completion)** blocks on: #161 (REPL/parsing)
  - **#160 (line editing)** blocks on: #161 (REPL/parsing)
  - **#155 (scripting)** blocks on: #161-#165 (core shell complete)
  - **#158 (job control)** blocks on: #161-#165 (core shell complete)
  - **#159 (advanced redirection)** blocks on: #164 (basic redirection)
- **Other Systems:**
  - **#106 (microkernel IPC)** blocks on: #101 (timers - CLOSED), #104 (shared memory)
  - **#107 (capability security)** blocks on: #106 (microkernel IPC)
  - **#105 (Unix sockets)** blocks on: #71 (socket API)
  - **#95 (userspace malloc)** blocks on: #89 (CLOSED) - ready!
  - **#111 (advanced pthread sync)** blocks on: #109 (pthread API)
  - **#112 (thread debugging)** blocks on: #109 (pthread API)
  - **#113 (thread-aware syscalls)** blocks on: #109 (pthread API)

### Parallel Development Opportunities:
- **mosh Shell** (#161-#165) progressing - REPL and exec complete; continue toward built-ins and I/O
  - Sequential: ✅ #161 (REPL) → ✅ #162 (exec) → #163 (built-ins) → #164 (redirection) → #165 (pipes)
  - Quality of life: #156 (history), #157 (tab), #160 (editing)
  - Advanced: #155 (scripting), #158 (job control), #159 (advanced redirection)
- **Threading APIs** (#109-#113) can start immediately - foundation complete!
- **Unicode Support** (#127-#134) can develop independently - start with #127!
- **Code Coverage** (#135) can develop immediately with existing Unity tests
- **Device Filesystem** (#136-#142) can start now with #136!
- **Mouse Input** (#143-#144) can develop independently from keyboard input
- **Virtual Filesystems** (#145-#148) can start immediately - VFS ready!
  - #145 (tmpfs) - Quick win, good first issue
  - #146 (devfs) - Integrates with device infrastructure
  - #147 (procfs) - System introspection
  - #148 (ext2) - Better persistent storage
- **Storage Systems** - PCI/AHCI infrastructure (#114-#120) active development
- **USB Infrastructure** (#121-#126) can develop independently
- **Networking stack** (#67-#73) can develop independently
- **SMP support** (#80-#84) can develop in parallel with IPC
- **jemalloc research** (#87-#92) can happen in parallel

## 🎯 Recommended Focus Areas

### Immediate Next Steps (Ready Now!)
1. **#145** - tmpfs/ramfs (quick win, good first issue, enables /tmp)
2. **#109** - pthread API and POSIX threading (foundation complete)
3. **#146** - devfs (device filesystem, unblocks hardware device access)
4. **#127** - UTF-8 utilities (ready to implement, no dependencies!)
5. **#135** - Code coverage with Gcov (ready to implement, existing Unity tests!)
6. **#136** - Device filesystem infrastructure (ready to implement!)
7. **#147** - procfs (system introspection and debugging)
8. **#102** - pipes implementation (needed for #165 pipe support)
9. **#103** - UNIX signals (process control ready: sigaction/masks live)

### For Maximum Impact:
1. **Complete Threading APIs** (#109, #110, #113) - Enable modern multithreaded applications
2. **Implement Advanced IPC** (#102, #103, #104) - Complete process communication
3. **Add Unicode Support** (#127, #128, #129) - International text handling

### For Running Applications (like text editors):
1. **File I/O**: #96 (CLOSED) → #60 (CLOSED) with filesystem chain (COMPLETE)
2. **Process management**: #93 (CLOSED) → #94 (signals - ready)
3. **Memory**: #89 (CLOSED) → #95 (userspace malloc - ready)
4. **Threading**: #108 (CLOSED) → #109 (pthread API - ready)
5. **Device I/O**: #136 → #138 (console/terminal devices)

### For Graphical Applications (like Doom):
1. **Graphics**: #31 (CLOSED) - Framebuffer syscalls (getinfo/map/flip) - COMPLETE
2. **Input**: #32 (CLOSED) - Keyboard support COMPLETE, #143/#144 - Mouse support pending
3. **Audio**: #33 - Audio subsystem for sound effects and music
4. **File I/O**: #60 (CLOSED) + #61 - Read/write for assets and save games
5. **Memory**: #89 (CLOSED) + #95 - Large allocations for game data

### For Unicode & International Support:
1. **Phase 1**: #127 (UTF-8 utilities) → #128 (fonts) → #129 (rendering)
2. **Phase 2**: #130 (keyboard input) + #131 (normalization)
3. **Phase 3**: #132 (filesystem) → #133 (locale/i18n) → #134 (testing)

### For Device Filesystem & Hardware Access:
1. **Phase 1**: #136 (device infrastructure) → #137 (null/zero)
2. **Phase 2**: #138 (console) + #140 (kbd/fb) + #141 (block devices)
3. **Phase 3**: #139 (random) + #142 (serial) → Complete /dev system

### For Mouse Input Support:
1. **Phase 1**: #143 (PS/2 mouse driver) → #32 (input subsystem integration)
2. **Phase 2**: #144 (USB mouse via HID) → #125 (USB HID driver) → Mouse events

### For Virtual Filesystems & System Services:
1. **Phase 1**: #145 (tmpfs/ramfs) → /tmp for temporary files (2-3 days)
2. **Phase 2**: #146 (devfs) → /dev for device access (3-5 days, integrates with #136-#142)
3. **Phase 3**: #147 (procfs) → /proc for system monitoring (4-6 days)
4. **Phase 4**: #148 (ext2) → Better persistent storage (1-2 weeks)

### For mosh Shell (Interactive System):
**Prerequisites (COMPLETE):**
- #149 (wait/waitpid - CLOSED)
- #150 (zombie handling - CLOSED)
- #153 (init program - CLOSED)
- #154 (boot integration - CLOSED)

**Phase 1: Shell Prerequisites (Week 1 - COMPLETE)**
1. #151 (getcwd/chdir - CLOSED)
2. #152 (environment variables - CLOSED)

**Phase 2: Core Shell (Week 2-3 - Tier 11 - 2-3 weeks)**
3. #161 (REPL & parsing) - 2-3 days
4. #162 (command execution) - 2-3 days
5. #163 (built-in commands) - 2-3 days
6. #164 (basic I/O redirection) - 2-3 days
7. #165 (pipe support) - 3-4 days
**Milestone: Basic working shell with pipes**

**Phase 3: Quality of Life (Week 4-5 - Tier 12 - 1-2 weeks)**
8. #156 (command history) - 3-5 days
9. #157 (tab completion) - 4-6 days
10. #160 (line editing keys) - 4-6 days
**Milestone: Comfortable interactive shell**

**Phase 4: Advanced Features (Week 6+ - Tier 12 - Optional)**
11. #155 (scripting) - 1-2 weeks
12. #158 (job control) - 1-2 weeks
13. #159 (advanced redirection) - 3-5 days
**Milestone: Full-featured shell**

**Result**: Working interactive shell in 2-3 weeks, full-featured in 4-6 weeks

### For Multi-User System (Future - 4-6 months):
**Prerequisites:**
- mosh shell complete (#161-#165)
- File system infrastructure solid (#145-#148)
- Core applications working

**Phase 1: User Infrastructure (Month 1)**
1. #166 (user/group database) - 3-5 days
2. #167 (process credentials) - 3-5 days
3. #168 (database parsing) - 3-5 days
4. #169 (login program) - 1-2 weeks
5. #170 (session management) - 1 week

**Phase 2: Permissions (Month 2)**
6. #171 (permission bits) - 1 week
7. #172 (VFS permission checking) - 1-2 weeks
8. #173 (security syscalls) - 1 week
9. #174 (syscall updates) - 2-3 weeks

**Phase 3: User Tools (Month 3)**
10. #175 (user utilities) - 2-3 weeks
11. #176 (su/sudo) - 1-2 weeks

**Phase 4: Security (Month 4)**
12. #177 (resource limits) - 1-2 weeks
13. #178 (security audit) - 2-3 weeks

**Result**: Proper multi-user operating system with authentication, permissions, and user isolation

**Priority**: LOW - implement after shell and core applications work
**Details**: See `docs/roads/road_to_multiuser.md` for comprehensive documentation

### For Microkernel Vision:
1. **Complete Phases 1-3** first (foundation + threading)
2. **Transition**: #106 → #107 (microkernel IPC + security)
3. **Architecture**: #97 for multi-platform support

## 📈 Progress Assessment

### **Completed (Major Components)**:
- **Foundation**: Memory management (#35, #57), scheduling (#34), synchronization (#36-#40)
- **Process Management**: File descriptors (#96), mmap/munmap (#89), fork/exec (#93)
- **Storage Stack**: Block drivers (#62), cache (#63), filesystems (#64), VFS (#65), syscalls (#60)
- **Threading Foundation**: Kernel threading infrastructure (#108)
- **Hardware**: PCI/AHCI controller (#114-#117), input subsystem (#32), framebuffer interface (#31)

### **Ready to Implement (High Impact)**:
- #152 (environment vars - CLOSED) - PATH lookup delivered
- #145 (tmpfs/ramfs) - Quick win, enables /tmp (2-3 days)
- #146 (devfs) - Device filesystem for /dev (3-5 days)
- #147 (procfs) - System introspection (4-6 days)
- #109 (pthread API) - Threading foundation complete
- #127 (UTF-8 utilities) - No dependencies
- #135 (code coverage) - Quality assurance
- #136 (device filesystem) - Hardware access
- #137 (null/zero devices) - Good first issue
- #143 (PS/2 mouse) - Input expansion
- #148 (ext2) - Better persistent storage (1-2 weeks)
- #102 (pipes) - Basic IPC
- #103 (UNIX signals) - Process control (handlers, masks, stoppable signals)

### **Project Status**:
- **Total Issues**: 178 issues planned (highest #178, includes future multi-user system)
- **Created Issues**: 165 issues (highest #165)
- **Closed**: 59 issues (major systems operational including wait/waitpid, zombies, init, boot)
- **Open**: 106 issues (organized by priority tiers)
- **Planned**: 13 issues for multi-user system (Tier 13 - #166-#178)
- **Major Completions**: Memory, scheduling, processes, storage, threading foundation, framebuffer, timers, signals, init supervisor
- **Active Development**: mosh shell (11 new issues), virtual filesystems, threading APIs, device filesystem, mouse input, hardware drivers, advanced IPC
- **Future Development**: Multi-user system infrastructure (see `docs/roads/road_to_multiuser.md`)

## **Current Development Strategy**

With core kernel infrastructure operational, meniOS has strong foundations for advanced features. Major systems like memory management, scheduling, processes, and storage are complete and functional.

**Recommended immediate development tracks:**
1. **mosh Shell** (#161-#165) - REPL and exec online; continue toward usability (TOP PRIORITY)
   - Built-in commands (2-3 days)
   - I/O redirection (2-3 days)
   - Pipe support (3-4 days)
   - Quality of life: history, tab completion, line editing (1-2 weeks)
   - **Result: Working interactive shell with pipes in ~2 weeks, full-featured in 4-6 weeks**
2. **Virtual Filesystems** (#145, #146, #147, #148) - Essential system services
   - tmpfs for /tmp (quick win)
   - devfs for /dev (hardware access)
   - procfs for /proc (debugging)
   - ext2 for better persistent storage
3. **Threading APIs** (#109, #110, #113) - Enable multithreaded applications
4. **Advanced IPC** (#102, #103, #104) - Complete process communication
5. **Unicode Support** (#127, #128, #129, #130) - International text handling
6. **Device Infrastructure** (#136, #137, #138) - Hardware access layer
7. **Mouse Input** (#143, #144) - Complete input subsystem with mouse support
8. **Quality Assurance** (#135) - Code coverage and testing improvements
9. **Hardware Drivers** (ongoing #118-#126) - USB and storage expansion

**Future Tracks (Post-Shell):**
10. **Multi-User System** (#166-#178) - Authentication, permissions, user isolation (4-6 months)
    - See `docs/roads/road_to_multiuser.md` for comprehensive roadmap
    - LOW PRIORITY - implement after shell and core applications

This parallel approach leverages the completed foundation to enable sophisticated userland applications including text editors, shells, and eventually Doom.

**Major Achievement**: Core kernel systems are operational with a complete storage stack, process management, init supervisor, and threading foundation ready for userland APIs and applications.
