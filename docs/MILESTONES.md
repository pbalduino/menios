# meniOS Milestones

This document tracks the three major milestones for meniOS development.

## 📊 Milestone Overview

### 1. **Mosh** (Shell Milestone)
**Goal**: Complete interactive shell with full user experience
**GitHub Milestone**: [Mosh](https://github.com/pbalduino/menios/milestone/1)

**Status**: 25/28 complete (89.3%)

**Assigned Issues**:

#### Core Shell Features (10 issues)
- [x] #54 - Evolve mosh shell to be default userland shell ✅
- [x] #162 - Command execution (fork/exec/wait) ✅
- [x] #163 - Built-in commands (cd/pwd/exit/export) ✅
- [x] #161 - Basic REPL and command parsing ✅
- [x] #148 - Environment variables support ✅
- [x] #180 - Environment seeding in init ✅
- [x] #185 - PATH search configuration ✅
- [x] #188 - /bin/env utility ✅
- [x] #204 - Logical operators (&&/||) ✅
- [x] #224 - unset built-in command ✅

#### I/O & Pipelines (4 issues)
- [x] #186 - Pipeline placeholder recognition ✅
- [x] #208 - Shell pipelines integration ✅
- [x] #164 - Basic I/O redirection (>, <) ✅
- [ ] #159 - Advanced redirection (2>&1, here-docs)

#### Utilities (4 issues)
- [x] #183 - /bin utility set (echo, cat, env, true, false) ✅
- [x] #187 - /bin/ps and /bin/kill utilities ✅
- [x] #181 - tmpfs validation ✅
- [x] #182 - waitpid regression test ✅

#### UX Features (9 issues)
- [x] #160 - Line editing keys (Ctrl+L/K/U/A/E/R) ✅
- [x] #184 - Line editor coverage ✅
- [x] #156 - Command history (up/down arrows) ✅
- [x] #197 - Tab completion for files/directories ✅
- [x] #198 - Ctrl+A/E line editing shortcuts ✅
- [x] #199 - Ctrl+R reverse search ✅
- [x] #200 - Ctrl+L clear screen ✅
- [x] #147 - getcwd/chdir syscalls ✅
- [x] #222 - Current directory in prompt ✅

#### Advanced Features (1 issue)
- [x] #158 - Job control (bg/fg/Ctrl-Z) ✅

#### Core Infrastructure (22 completed dependencies)
- [x] #8 - ANSI and scrolling console ✅
- [x] #25 - Caret rendering fix ✅
- [x] #26 - ELF loader ✅
- [x] #27 - Syscall ABI and dispatcher ✅
- [x] #43 - GDT user segments ✅
- [x] #44 - TSS implementation ✅
- [x] #45 - User/kernel memory protection ✅
- [x] #46 - Ring 0/Ring 3 transitions ✅
- [x] #48 - Basic syscall interface ✅
- [x] #50 - SYS_WRITE syscall ✅
- [x] #51 - SYS_EXIT syscall ✅
- [x] #57 - Virtual memory manager ✅
- [x] #60 - Filesystem syscalls ✅
- [x] #61 - Userspace filesystem integration ✅
- [x] #62 - Block device driver ✅
- [x] #63 - Block cache ✅
- [x] #64 - Filesystem library (FAT32) ✅
- [x] #65 - VFS layer ✅
- [x] #93 - Fork/exec process creation ✅
- [x] #96 - File descriptor management ✅
- [x] #102 - Pipes implementation ✅

#### Bug Fixes (2 issues)
- [x] #94 - Implement signal handling and delivery system ✅
- [ ] #202 - /dev/zero EOF bug

**Dependencies**:
- #187 depends on #213 (signal support for kill) ✅ COMPLETE
- #197 depends on #147 (getcwd/chdir) ✅ COMPLETE
- #222 depends on #147 (getcwd/chdir) ✅ COMPLETE
- #199 depends on #156 (history) ✅ COMPLETE

**Notes**:
- Closed #157 as duplicate of #197 (tab completion)
- Closed #165 as completed by #208 (pipe support)
- Moved #143 to Doom milestone (mouse driver)
- Removed #201 (mouse selection - depends on #143), #177, #172, #166, #155 from Mosh milestone

---

### 2. **GCC** (Toolchain Milestone)
**Goal**: Enable native compilation on meniOS with GCC toolchain support
**GitHub Milestone**: [GCC](https://github.com/pbalduino/menios/milestone/2)

**Status**: 2/8 complete (25%)

**Assigned Issues**:
- [x] #192 - crt0 runtime startup code ✅
- [x] #193 - Minimal userland libc ✅
- [ ] #194 - Syscall ABI documentation (ready now!)
- [ ] #195 - Userland build system (ready now!)
- [ ] #29 - Cross-compiler toolchain integration
- [ ] #190 - TCC (Tiny C Compiler) port
- [ ] #191 - binutils (as, ld) port
- [ ] #196 - Fish shell research

**Critical Path**: #192 ✅ → #193 ✅ → #194 → #195 → #29 → #190/#191

**Dependencies**:
- #29 requires #194, #195
- #190 requires #29, #189 (FAT32 writes)
- #191 requires #29, #189 (FAT32 writes)

**Progress**: Foundation complete! crt0 and libc are done, ready for ABI docs and build system.

---

### 3. **Doom** (Game Porting Milestone)
**Goal**: Run Doom (1993) in userland on meniOS
**GitHub Milestone**: [Doom](https://github.com/pbalduino/menios/milestone/3)

**Status**: 10/26 complete (38.5%)

**Assigned Issues**:

#### Graphics & Audio (4 issues)
- [x] #31 - Userspace graphics interface ✅
- [x] #32 - Input subsystem ✅
- [ ] #33 - Audio subsystem
- [ ] #143 - Mouse driver

#### Threading Support (5 issues)
- [ ] #109 - pthread API implementation (ready now!)
- [ ] #110 - Thread-safe C library
- [ ] #111 - Advanced pthread synchronization
- [ ] #112 - Thread debugging and profiling
- [ ] #113 - Thread-aware system calls

#### Memory & Process (1 issue)
- [ ] #95 - Userspace memory allocator (malloc/free)

#### File System (1 issue)
- [ ] #189 - FAT32 write support

#### IPC - Pipes (1 issue - parent)
- [x] #102 - Pipes/FIFOs implementation ✅ COMPLETE

#### IPC - Signals (6 issues)
- [x] #103 - UNIX signals (parent issue) ✅
- [x] #210 - Signal bookkeeping scaffold ✅
- [x] #211 - Signal syscalls ✅
- [x] #212 - Signal delivery path ✅
- [x] #213 - Shell Ctrl+C integration ✅
- [ ] #214 - Advanced signal features

#### IPC - Shared Memory (5 issues) ✅ COMPLETE!
- [x] #215 - Shared memory manager ✅
- [x] #216 - Shared memory syscalls ✅
- [x] #217 - Reference counting and cleanup ✅
- [x] #218 - Comprehensive test suite ✅
- [x] #219 - Documentation and examples ✅

#### IPC - Other (4 issues)
- [ ] #105 - Unix domain sockets
- [ ] #106 - Microkernel message passing IPC
- [ ] #107 - Capability-based security
- [x] #220 - ioctl syscall ✅
- [ ] #221 - Fast syscall instruction (ready now!)

**Dependencies**:
- #110 requires #109
- #111 requires #109
- #213 requires #212 ✅ COMPLETE
- #214 requires #213, #109
- #106 requires #102 ✅, #103 ✅ (pipes & signals)
- #107 requires #106

**Progress**: IPC infrastructure well underway - Pipes ✅, Signals (5/6), Shared Memory ✅ all complete!

---

## 🎯 Dependency Flow Between Milestones

```
┌─────────────────────────────────────────┐
│  Mosh (Shell UX & Developer Tools)      │
│  - Interactive shell complete            │
│  - Basic utilities working               │
│  - Development environment ready         │
└────────────────┬────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────┐
│  GCC (Native Compilation Toolchain)      │
│  - Cross-compiler working                │
│  - Can compile C programs for meniOS     │
│  - Eventually native compilation         │
└────────────────┬────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────┐
│  Doom (Full OS Capabilities)             │
│  - Graphics, audio, threading            │
│  - Complete IPC support                  │
│  - Can run complex userland applications │
└─────────────────────────────────────────┘
```

## 📈 Overall Progress

- **Total Issues Across Milestones**: 62 issues
- **Completed**: 37 issues (59.7%)
- **In Progress**: 25 issues
- **Ready to Start**: 2 issues (no dependencies)

## 🚀 Immediate Next Steps

### Ready to Start Now (No Dependencies):
1. **GCC Milestone**:
   - #194 - Syscall ABI docs
   - #195 - Userland build system

2. **Doom Milestone**:
   - #109 - pthread API
   - #221 - Fast syscall instruction

## 📝 Notes

### Parallel Development
Many issues can be worked on in parallel:
- **GCC**: #194 and #195 can be done simultaneously
- **Doom Threading**: #109, #112, #113 are independent
- **Doom IPC**: Different IPC mechanisms can progress in parallel
- **Mosh UX**: #198 ✅, #199 ✅, #200 ✅, #197 ✅ are independent features (all complete!)

### Critical Dependencies
- **GCC milestone** is required before meniOS can compile Doom natively
- **Mosh milestone** provides the development environment for debugging
- Many **Doom** features are prerequisites for running the game

### Completion Order
Recommended completion order for maximum impact:
1. **Mosh** - Provides usable development environment (89.3% complete)
2. **GCC** - Enables native development and compilation (25% complete)
3. **Doom** - Demonstrates full OS capabilities (38.5% complete)

### Recent Changes
- **2025-10-08**: Expanded Mosh milestone from 10 to 30 issues to better track all shell work
- **2025-10-08**: Added #192, #193 to GCC milestone (already complete)
- **2025-10-08**: Added #95, #103, #105-#107 to Doom milestone for comprehensive IPC tracking
- **2025-10-08**: Closed #147 (getcwd/chdir) - unblocked #197 and #222
- **2025-10-11**: Closed #222 (current directory in prompt)
- **2025-10-11**: Closed #188 (/bin/env utility)
- **2025-10-11**: Closed #203 (pipeline hang bug)
- **2025-10-11**: Closed #198 (Ctrl+A/E shortcuts), #200 (Ctrl+L clear screen)
- **2025-10-11**: Closed #156 (command history) - unblocks #199!
- **2025-10-11**: Closed #197 (tab completion) ✅
- **2025-10-11**: Closed #199 (Ctrl+R reverse search) ✅
- **2025-10-11**: Closed #213 (Shell Ctrl+C integration) ✅
- **2025-10-11**: Closed #161 (Basic REPL and command parsing) ✅
- **2025-10-11**: Closed #164 (Basic I/O redirection) ✅
- **2025-10-11**: Closed #187 (/bin/ps and /bin/kill utilities) ✅
- **2025-10-11**: Closed #204 (Logical operators &&/||) ✅
- **2025-10-11**: Created #224 (unset built-in command)
- **2025-10-11**: Closed #158 (Job control bg/fg/Ctrl-Z) ✅
- **2025-10-11**: Closed #224 (unset built-in command) ✅
- **2025-10-09**: Moved #143 (mouse driver) to Doom milestone
- **2025-10-09**: Removed #201, #177, #172, #166, #155 from Mosh milestone
- **2025-10-09**: Mosh milestone now focuses on core shell functionality: #94, #159, #202
- **2025-10-12**: Closed #94 (signal handling); Mosh milestone now tracks #159 and #202
- **2025-10-08**: Closed #157 as duplicate of #197, #165 as completed by #208

---

**Last Updated**: 2025-10-09
**See Also**:
- [Road to Shell](road/road_to_shell.md)
- [Road to GCC](road/road_to_gcc.md)
- [Road to Doom](road/road_to_doom.md)
