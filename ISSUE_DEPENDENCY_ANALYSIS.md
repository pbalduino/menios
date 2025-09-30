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
30. **#138** - /dev/console and terminal devices (CLOSED)
31. **#139** - /dev/random and /dev/urandom entropy devices
32. **#140** - /dev/kbd0 and /dev/fb0 hardware devices
33. **#141** - /dev/sda block devices for storage
34. **#142** - /dev/ttyS0 serial device interface

### 🆕 Tier 8: Mouse Input Support (NEW!)
35. **#143** - PS/2 mouse driver and input support
36. **#144** - USB mouse support through HID class driver

### 🆕 Tier 9: init & Shell Infrastructure (NEW!)
37. **#145** - wait/waitpid syscall (process synchronization)
38. **#146** - Process zombie state and orphan reparenting (CLOSED)
39. **#147** - getcwd/chdir syscalls (directory navigation)
40. **#148** - Environment variables support (getenv/setenv/unsetenv)
41. **#149** - Barebone init program (PID 1 process supervisor - CLOSED)
42. **#150** - Boot integration to start init as PID 1 (CLOSED)

### 🆕 Tier 10: Virtual & Special Filesystems (NEW!)
43. **#151** - tmpfs/ramfs (in-memory filesystem for /tmp - CLOSED)
44. **#152** - devfs (device filesystem for /dev hierarchy - CLOSED)
45. **#153** - procfs (process information filesystem for /proc - CLOSED)
46. **#154** - ext2 filesystem support (read-only initially - CLOSED)

### Tier 11: mosh Shell Core Components
47. **#155** - Scripting support (if/while/for/functions)
48. **#156** - Command history (up/down arrows)
49. **#157** - Tab completion
50. **#158** - Job control (bg/fg/Ctrl-Z)
51. **#159** - Advanced redirection (2>&1, here-docs)
52. **#160** - Line editing keys (Ctrl+L/K/U/A/E/R)
53. **#161** - Basic REPL and command parsing (read/parse/execute loop - CLOSED)
54. **#162** - Command execution (fork/exec/wait infrastructure - CLOSED)
55. **#163** - Built-in commands (cd/pwd/exit/export)
56. **#164** - Basic I/O redirection (>, <, >>)
57. **#165** - Pipe support (|)
58. **#166** - Hook shell I/O to virtual terminals/VGA (TTY devices, ANSI codes - CLOSED)

### Tier 12: Terminal Infrastructure (NEW!)
59. **#168** - Console/VGA driver infrastructure (text mode, 80×25)
60. **#169** - TTY subsystem (line discipline, virtual terminals, ioctl - CLOSED)
61. **#170** - Character device infrastructure (foundation for streaming devices)

### Tier 13: Multi-User System Infrastructure (Future - Not Yet Created)
62. **TBD** - User/group database infrastructure (/etc/passwd, /etc/group)
63. **TBD** - Process credentials (UID/GID per process)
64. **TBD** - User database parsing and management
65. **TBD** - Login program and authentication
66. **TBD** - Session management and getty
67. **TBD** - File system permission bits (owner/group/other)
68. **TBD** - VFS permission checking
69. **TBD** - Security syscalls (getuid/setuid family)
70. **TBD** - Update all syscalls for permission checks
71. **TBD** - User management utilities (useradd, passwd, chmod)
72. **TBD** - su and sudo implementation
73. **TBD** - Resource limits (ulimit/getrlimit/setrlimit)
74. **TBD** - Security testing and audit

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
Phase 3: Hardware        │      #138 (console/tty - CLOSED) ──┐    │
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
Phase 2: In-Memory FS        #151 (tmpfs/ramfs - CLOSED) → /tmp for temporary files
                                  ↓
Phase 3: Device FS           #152 (devfs - CLOSED) → Integrates with #136-#142 (/dev hierarchy)
                                  ↓                   Exposes devices as files
                                  ↓
Phase 4: Process Info        #153 (procfs - CLOSED) → /proc for system introspection
                                  ↓                    Process monitoring/debugging
                                  ↓
Phase 5: Block FS            #154 (ext2 - CLOSED) → Better persistent storage
#62 (block driver) ──────────────┘                  Alternative to FAT32
#63 (block cache) ───────────────┘                  Native Linux filesystem
```

### 🆕 init & Shell Chain (NEW!)
```
Phase 1: Process Synchronization
#93 (fork/exec - CLOSED) ──┐
#60 (syscalls - CLOSED) ────┼──→ #145 (wait/waitpid) → Process can wait for children
                            │            ↓
Phase 2: Zombie Handling    │      #146 (zombie state + orphan reparenting - CLOSED)
                            │            ↓              Parent reaps children
                            │            ↓              Orphans go to init
Phase 3: Shell Support      │            ↓
#65 (VFS - CLOSED) ─────────┼──→   #147 (getcwd/chdir) → Directory navigation
                            │            ↓
                            └──→   #148 (environment vars) → PATH, HOME, etc.
                                         ↓
Phase 4: init Program                    ├──→ #149 (init program - CLOSED)
#145 (wait/waitpid) ──────────────────────────┘         ↓  Reaps zombies
#146 (zombie handling - CLOSED) ───────────────────┘  Supervises processes
                                                    ↓
Phase 5: Boot Integration                    #150 (boot init as PID 1 - CLOSED)
                                                    ↓  Start init at boot (DONE)
                                                    ↓
```

### mosh Shell Component Chain
```
Phase 1: Core Infrastructure (Tier 11 - Critical)
#145 (wait) ──────────────────┐
#147 (getcwd/chdir) ──────────┼──→ #161 (REPL & parsing - CLOSED) → Read, parse, tokenize commands
#148 (environment) ───────────┘          ↓
                                   ↓
                               #162 (command execution - CLOSED) → fork/exec/wait/PATH lookup
                                   ↓
                              #163 (built-ins) → cd/pwd/exit/export

Phase 2: I/O & Pipes (Tier 11 - High Priority)
#96 (file descriptors - CLOSED) → #164 (basic redirection) → >, <, >>
#102 (pipes) ────────────────────→ #165 (pipe support) → cmd1 | cmd2 | cmd3

Phase 3: Terminal Integration (Tier 12 - Essential)
#170 (char device infra) ──→ #168 (VGA driver) ──→ #169 (TTY subsystem - CLOSED) ──→ #166 (shell VT/VGA - CLOSED) → Console bridge in place
                         └──→ #137 (/dev/null/zero) ─┘                                              (full VT stack pending)

Phase 4: Usability Features (Tier 11 - Quality of Life)
#169 (TTY - CLOSED) ──→ #156 (command history) → Up/down arrows, Ctrl+R
                    ├─→ #157 (tab completion) → Complete commands/files
                    └─→ #160 (line editing) → Ctrl+L/K/U/A/E/R

Phase 5: Advanced Features (Tier 11 - Optional)
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
                          ├──→ User/group database (TBD) → /etc/passwd, /etc/group, /etc/shadow
                          │          ↓
#93 (fork/exec - CLOSED) ─┼──→ Process credentials (TBD) → UID/GID per process
                          │          ↓
                          └──→ Database parsing utilities (TBD) → Read user/group data

Phase 2: Authentication
Credentials (TBD) ───────→ Login program (TBD) → Username/password auth
                                    ↓
                               Session management (TBD) → Getty, TTY allocation

Phase 3: File Permissions
#65 (VFS - CLOSED) ───────→ Permission bits (TBD) → Owner/group/other rwx
                                    ↓
Credentials (TBD) ───────→ Permission checking (TBD) → Enforce access control

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
- **#161 (REPL - CLOSED)** - Shell parsing delivered
- **#162 (command exec - CLOSED)** - Shell execution delivered
- **#163 (built-ins)** - Ready: cd/pwd/exit/export commands
- **#164 (basic redirection)** - Ready: >, <, >> operators
- **#165 (pipe support)** - Needs: #102 (pipes syscall)
- **#170 (char device infra)** - Ready: foundation for all streaming devices
- **#137 (/dev/null and /dev/zero)** - Needs: #170 (char device), #152 (devfs - CLOSED)
- **#168 (VGA driver)** - Ready: text mode console support
- **#169 (TTY subsystem - CLOSED)** - Canonical input, echo, and `/dev/tty0`
- **#166 (shell VT/VGA - CLOSED)** - Initial console bridge in place (full VT awaits #168/#170)
- **#109 (pthread API)** - Dependencies met: #108 (CLOSED)
- **#110 (thread-safe libc)** - Can start in parallel with #109
- **#127 (UTF-8 utilities)** - No dependencies, ready to start immediately!
- **#135 (code coverage)** - Can start with existing Unity tests, no blocking dependencies
- **#136 (device filesystem)** - Dependencies: #96 (CLOSED), #60 (CLOSED) - ready!
- **#145 (wait/waitpid)** - Process synchronization syscall
- **#146 (zombie/orphan - CLOSED)** - Zombie process state and reparenting
- **#147 (getcwd/chdir)** - Directory navigation syscalls
- **#148 (environment vars)** - Environment variable support
- **#102 (pipes)** - Basic IPC implementation
- **#103 (UNIX signals)** - Process control mechanism (handlers delivered; siginfo/rt signals TBD)

### Cannot Start Until Complete:
- **Shell Core Components:** (Most unblocked!)
  - **#163 (built-ins)** - Ready now (REPL/exec complete)
  - **#164 (basic redirection)** - Ready now (REPL/exec complete)
  - **#165 (pipe support)** blocks on: #102 (pipes syscall)
  - **#166 (shell VT/VGA - CLOSED)** now uses /dev/console; richer VT work continues in #168/#170
- **Terminal Infrastructure:**
  - **#137 (/dev/null+zero)** blocks on: #170 (char device)
  - **#169 (TTY subsystem - CLOSED)** delivered canonical input; advanced VT work continues under #168/#170
  - **#166 (shell VT/VGA - CLOSED)** complete for console output; backed by new TTY layer (#169)
- **Shell Advanced Features:**
  - **#156 (command history)** builds on #169 (TTY - CLOSED) for raw mode switching
  - **#157 (tab completion)** blocks on: #163 (built-ins) for context
  - **#160 (line editing)** can now target the raw mode exposed by #169 (TTY - CLOSED)
  - **#155 (scripting)** blocks on: #163-#165 (core shell complete)
  - **#158 (job control)** blocks on: #103 (signals), #163-#165 (core shell)
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
- **mosh Shell** (#163-#166) progressing - REPL/exec/TTY hookup complete; continue toward built-ins and I/O
  - Sequential: ✅ #161 (REPL) → ✅ #162 (exec) → #163 (built-ins) → #164 (redirection) → #165 (pipes)
  - Terminal: #170 (char dev) → #168 (VGA) → #166 (shell integration - CLOSED, runs atop #169 TTY)
  - Quality of life: #156 (history), #157 (tab), #160 (editing)
  - Advanced: #155 (scripting), #158 (job control), #159 (advanced redirection)
- **Init & Process Management** (#145-#148) - Process lifecycle syscalls
  - #145 (wait/waitpid - CLOSED), #146 (zombie/orphan - CLOSED), #147 (getcwd/chdir), #148 (environment)
- **Threading APIs** (#109-#113) can start immediately - foundation complete!
- **Unicode Support** (#127-#134) can develop independently - start with #127!
- **Code Coverage** (#135) can develop immediately with existing Unity tests
- **Device Filesystem** (#136-#142) can start now with #136!
- **Mouse Input** (#143-#144) can develop independently from keyboard input
- **Virtual Filesystems** (✅ #151-#154 COMPLETE!)
  - ✅ #151 (tmpfs - CLOSED)
  - ✅ #152 (devfs - CLOSED)
  - ✅ #153 (procfs - CLOSED)
  - ✅ #154 (ext2 - CLOSED)
- **Storage Systems** - PCI/AHCI infrastructure (#114-#120) active development
- **USB Infrastructure** (#121-#126) can develop independently
- **Networking stack** (#67-#73) can develop independently
- **SMP support** (#80-#84) can develop in parallel with IPC
- **jemalloc research** (#87-#92) can happen in parallel

## 🎯 Recommended Focus Areas

### Immediate Next Steps (Ready Now!)

**Top Priority - Shell Completion (1-2 weeks):**
1. **#163** - Built-in commands (cd/pwd/exit/export) - 2-3 days
2. **#164** - Basic I/O redirection (>, <, >>) - 2-3 days
3. **#102** - Pipes syscall implementation - 3-4 days
4. **#165** - Pipe support in shell (cmd1 | cmd2) - 2-3 days
   → **Result: Working shell with pipes!**

**High Priority - Terminal Support (1-2 weeks):**
5. **#170** - Character device infrastructure - 3-5 days
6. **#168** - VGA driver (text mode) - 3-5 days
7. **#169** - TTY subsystem - CLOSED (canonical input, `/dev/tty0`)
8. **#166** - Hook shell to VT/VGA - CLOSED
9. **#167** - /dev/zero device - 1-2 days
   → **Result: Full terminal experience!**

**Parallel Tracks (Can start anytime):**
10. **#151** - tmpfs/ramfs (quick win, enables /tmp - CLOSED)
11. **#152** - devfs (device filesystem - CLOSED)
12. **#109** - pthread API (foundation complete) - 1-2 weeks
13. **#127** - UTF-8 utilities (no dependencies) - 1 week

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
2. **Phase 2**: #138 (console - CLOSED) + #140 (kbd/fb) + #141 (block devices)
3. **Phase 3**: #139 (random) + #142 (serial) → Complete /dev system

### For Mouse Input Support:
1. **Phase 1**: #143 (PS/2 mouse driver) → #32 (input subsystem integration)
2. **Phase 2**: #144 (USB mouse via HID) → #125 (USB HID driver) → Mouse events

### For Virtual Filesystems & System Services:
1. **Phase 1**: #151 (tmpfs/ramfs - CLOSED) → /tmp for temporary files
2. **Phase 2**: #152 (devfs - CLOSED) → /dev for device access (integrates with #136-#142)
3. **Phase 3**: #153 (procfs - CLOSED) → /proc for system monitoring
4. **Phase 4**: #154 (ext2 - CLOSED) → Better persistent storage

### For mosh Shell (Interactive System):
**Prerequisites (COMPLETE):**
- #145 (wait/waitpid - CLOSED)
- #146 (zombie handling - CLOSED)
- #149 (init program - CLOSED)
- #150 (boot integration - CLOSED)

**Phase 1: Shell Prerequisites (Week 1 - COMPLETE)**
1. #147 (getcwd/chdir - CLOSED)
2. #148 (environment variables - CLOSED)

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
- File system infrastructure solid (#151-#154)
- Core applications working

**Phase 1: User Infrastructure (Month 1)**
1. User/group database (TBD) - 3-5 days
2. Process credentials (TBD) - 3-5 days
3. Database parsing tools (TBD) - 3-5 days
4. Login program (TBD) - 1-2 weeks
5. Session management/getty (TBD) - 1 week

**Phase 2: Permissions (Month 2)**
6. Permission bits on files (TBD) - 1 week
7. VFS permission checking (TBD) - 1-2 weeks
8. Security syscalls (getuid/setuid) (TBD) - 1 week
9. Syscall updates for permissions (TBD) - 2-3 weeks

**Phase 3: User Tools (Month 3)**
10. User management utilities (TBD) - 2-3 weeks
11. su/sudo implementation (TBD) - 1-2 weeks

**Phase 4: Security (Month 4)**
12. Resource limits (TBD) - 1-2 weeks
13. Security audit & hardening (TBD) - 2-3 weeks

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
- #148 (environment vars) - PATH/HOME defaults for userland
- #151 (tmpfs/ramfs - CLOSED) - In-memory /tmp landing
- #152 (devfs - CLOSED) - Device filesystem for /dev
- #153 (procfs - CLOSED) - System introspection surface
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
- **Total Issues**: 170 issues created (highest #170, note: #167 closed as duplicate)
- **Closed**: 60 issues (including #149, #150, #151-#154, #161, #162, #167)
- **Open**: 110 issues (organized by priority tiers)
- **Planned**: 13 issues for multi-user system (Tier 13 - to be created)
- **Major Completions**:
  - Core kernel: Memory, scheduling, processes, storage, threading foundation
  - Hardware: Framebuffer, keyboard input, timers, signals
  - Filesystems: tmpfs, devfs, procfs, ext2 (all read-only or complete)
  - Process management: Init supervisor, boot integration
  - Shell: REPL and command execution
- **Active Development**:
  - **mosh shell** (core: #163-#165 built-ins/I/O/pipes; terminal: #168-#170 VGA/TTY/char dev; advanced: #155-#160)
  - **Process lifecycle** (#145-#148: wait/waitpid, zombies, getcwd/chdir, environment)
  - **Terminal infrastructure** (#168-#170: VGA driver, TTY, character devices)
  - **Device filesystem** (#136-#142: /dev hierarchy devices)
  - **Threading APIs** (#109-#113: pthread, thread-safe libc)
  - **Mouse input** (#143-#144: PS/2 and USB mouse)
  - **Advanced IPC** (#102-#107: pipes, signals, shared memory)
- **Future Development**: Multi-user system infrastructure (see `docs/roads/road_to_multiuser.md`)

## **Current Development Strategy**

With core kernel infrastructure operational, meniOS has strong foundations for advanced features. Major systems like memory management, scheduling, processes, and storage are complete and functional.

**Recommended immediate development tracks:**
1. **mosh Shell** (#163-#166) - REPL/exec/console DONE; continue toward usability (TOP PRIORITY)
   - Built-in commands (#163) - 2-3 days
   - I/O redirection (#164) - 2-3 days
   - Pipe support (#102, #165) - 5-7 days total
   - Terminal integration (#170, #168) - 2-3 weeks
   - Quality of life: history, tab completion, line editing (#156, #157, #160) - 1-2 weeks
   - **Result: Working shell with pipes in ~2 weeks, full terminal in 4 weeks, polished in 6 weeks**
2. **Virtual Filesystems** (#151, #152, #153, #154) - Essential system services
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
10. **Multi-User System** (TBD) - Authentication, permissions, user isolation (4-6 months)
    - See `docs/roads/road_to_multiuser.md` for comprehensive roadmap
    - LOW PRIORITY - implement after shell and core applications

This parallel approach leverages the completed foundation to enable sophisticated userland applications including text editors, shells, and eventually Doom.

**Major Achievement**: Core kernel systems are operational with a complete storage stack, process management, init supervisor, and threading foundation ready for userland APIs and applications.
