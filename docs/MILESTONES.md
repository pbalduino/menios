# meniOS Milestones

This document tracks the three major milestones for meniOS development.

## 📊 Milestone Overview

### 1. **Mosh** (Shell Milestone)
**Goal**: Complete interactive shell with full user experience
**GitHub Milestone**: [Mosh](https://github.com/pbalduino/menios/milestone/1)

**Status**: 15/30 complete (50.0%)

**Assigned Issues**:

#### Core Shell Features (12 issues)
- [x] #54 - Evolve mosh shell to be default userland shell ✅
- [x] #162 - Command execution (fork/exec/wait) ✅
- [x] #163 - Built-in commands (cd/pwd/exit/export) ✅
- [ ] #161 - Basic REPL and command parsing
- [x] #148 - Environment variables support ✅
- [x] #180 - Environment seeding in init ✅
- [x] #185 - PATH search configuration ✅
- [x] #188 - /bin/env utility ✅

#### I/O & Pipelines (4 issues)
- [x] #186 - Pipeline placeholder recognition ✅
- [x] #208 - Shell pipelines integration ✅
- [ ] #164 - Basic I/O redirection (>, <)
- [ ] #159 - Advanced redirection (2>&1, here-docs)

#### Utilities (4 issues)
- [x] #183 - /bin utility set (echo, cat, env, true, false) ✅
- [ ] #187 - /bin/ps and /bin/kill utilities
- [x] #181 - tmpfs validation ✅
- [x] #182 - waitpid regression test ✅

#### UX Features (10 issues)
- [x] #160 - Line editing keys (Ctrl+L/K/U/A/E/R) ✅
- [x] #184 - Line editor coverage ✅
- [ ] #156 - Command history (up/down arrows)
- [ ] #197 - Tab completion for files/directories
- [ ] #198 - Ctrl+A/E line editing shortcuts
- [ ] #199 - Ctrl+R reverse search
- [ ] #200 - Ctrl+L clear screen
- [x] #147 - getcwd/chdir syscalls ✅
- [x] #222 - Current directory in prompt ✅
- [ ] #201 - Mouse selection/copy/paste

#### Advanced Features (3 issues)
- [ ] #155 - Scripting support (if/while/for/functions)
- [ ] #158 - Job control (bg/fg/Ctrl-Z)
- [ ] #166 - Hook shell I/O to virtual terminals/VGA

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

**Dependencies**:
- #187 depends on #213 (signal support for kill)
- #197 depends on #147 (getcwd/chdir) ✅ COMPLETE
- #222 depends on #147 (getcwd/chdir) ✅ COMPLETE
- #199 depends on #156 (history)
- #201 depends on #143 or #144 (mouse drivers)

**Notes**:
- Closed #157 as duplicate of #197 (tab completion)
- Closed #165 as completed by #208 (pipe support)

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

**Status**: 10/25 complete (40%)

**Assigned Issues**:

#### Graphics & Audio (3 issues)
- [x] #31 - Userspace graphics interface ✅
- [x] #32 - Input subsystem ✅
- [ ] #33 - Audio subsystem

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
- [ ] #213 - Shell Ctrl+C integration (ready now!)
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
- #213 requires #212 ✅
- #214 requires #213, #109
- #106 requires #102 ✅, #103 ✅ (pipes & signals)
- #107 requires #106

**Progress**: IPC infrastructure well underway - Pipes ✅, Signals (4/6), Shared Memory ✅ all complete!

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

- **Total Issues Across Milestones**: 63 issues
- **Completed**: 27 issues (42.9%)
- **In Progress**: 36 issues
- **Ready to Start**: 5 issues (no dependencies)

## 🚀 Immediate Next Steps

### Ready to Start Now (No Dependencies):
1. **GCC Milestone**:
   - #194 - Syscall ABI docs
   - #195 - Userland build system

2. **Doom Milestone**:
   - #109 - pthread API
   - #213 - Shell Ctrl+C integration
   - #221 - Fast syscall instruction

3. **Mosh Milestone**:
   - #198 - Ctrl+A/E shortcuts
   - #200 - Ctrl+L clear screen
   - #197 - Tab completion (dependency #147 now complete!)

## 📝 Notes

### Parallel Development
Many issues can be worked on in parallel:
- **GCC**: #194 and #195 can be done simultaneously
- **Doom Threading**: #109, #112, #113 are independent
- **Doom IPC**: Different IPC mechanisms can progress in parallel
- **Mosh UX**: #198, #199, #200, #197 are independent features

### Critical Dependencies
- **GCC milestone** is required before meniOS can compile Doom natively
- **Mosh milestone** provides the development environment for debugging
- Many **Doom** features are prerequisites for running the game

### Completion Order
Recommended completion order for maximum impact:
1. **Mosh** - Provides usable development environment (50% complete)
2. **GCC** - Enables native development and compilation (25% complete)
3. **Doom** - Demonstrates full OS capabilities (40% complete)

### Recent Changes
- **2025-10-08**: Expanded Mosh milestone from 10 to 30 issues to better track all shell work
- **2025-10-08**: Added #192, #193 to GCC milestone (already complete)
- **2025-10-08**: Added #95, #103, #105-#107 to Doom milestone for comprehensive IPC tracking
- **2025-10-08**: Closed #147 (getcwd/chdir) - unblocked #197 and #222
- **2025-10-11**: Closed #222 (current directory in prompt)
- **2025-10-11**: Closed #188 (/bin/env utility)
- **2025-10-11**: Closed #203 (pipeline hang bug)
- **2025-10-08**: Closed #157 as duplicate of #197, #165 as completed by #208

---

**Last Updated**: 2025-10-11
**See Also**:
- [Road to Shell](road/road_to_shell.md)
- [Road to GCC](road/road_to_gcc.md)
- [Road to Doom](road/road_to_doom.md)
