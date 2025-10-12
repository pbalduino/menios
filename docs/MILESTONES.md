# meniOS Milestones

This document tracks the three major milestones for meniOS development.

> **Release v0.1.0 (2025-10-13):** celebrates the completion of the Mosh shell milestone and is the first meniOS build to ship a polished interactive shell to users.

## 📊 Milestone Overview

### 1. **Mosh** (Shell Milestone)
**Goal**: Complete interactive shell with full user experience
**GitHub Milestone**: [Mosh](https://github.com/pbalduino/menios/milestone/1)

**Status**: 27/27 complete (100%)

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
 - [x] #159 - Advanced redirection (2>&1, append, fd duplication) ✅

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

#### Bug Fixes (completed)
- [x] #94 - Implement signal handling and delivery system ✅

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
- Deferred #202 (/dev/zero EOF bug) out of the Mosh milestone backlog

---

### 2. **Buddy Allocator** (Memory Management Milestone)
**Goal**: Migrate userland allocator from first-fit to buddy allocator system
**GitHub Milestone**: [Buddy Allocator](https://github.com/pbalduino/menios/milestone/4)

**Status**: 1/9 complete (11.1%)

**Assigned Issues**:
- [x] #245 - Survey Current Heap Implementation ✅
- [ ] #246 - Define Buddy Allocator Orders and Configuration
- [ ] #247 - Rewrite Arena Setup for Buddy Allocator
- [ ] #248 - Implement Buddy Split and Coalesce Operations
- [ ] #249 - Integrate Buddy Allocator with malloc/free
- [ ] #250 - Adapt realloc/reallocarray for Buddy Allocator
- [ ] #251 - Update Direct mmap Path for Large Allocations
- [ ] #252 - Add Buddy Allocator Diagnostics and Tests
- [ ] #253 - Cleanup and Document Buddy Allocator Migration

**Critical Path**: #245 → #246 → #247 → #248 → #249 → #250/#251 → #252 → #253

**Dependencies**:
- #246 depends on #245 (survey)
- #247 depends on #246 (configuration)
- #248 depends on #247 (arena setup)
- #249 depends on #248 (split/coalesce)
- #250 depends on #249 (malloc/free)
- #251 depends on #249 (malloc/free)
- #252 depends on #248, #249 (core operations + API)
- #253 depends on #252 (testing)

**Progress**: Survey complete (#245 ✅)! Current first-fit allocator fully documented. Next: Define buddy orders and configuration (#246). Both GCC and Doom milestones depend on buddy allocator completion for efficient handling of complex allocation patterns.

**Estimated Effort**: 20-28 days remaining (2-3 days complete)

---

### 3. **GCC** (Toolchain Milestone)
**Goal**: Enable native compilation on meniOS with GCC toolchain support
**GitHub Milestone**: [GCC](https://github.com/pbalduino/menios/milestone/2)

**Status**: 5/8 complete (62.5%)

**Assigned Issues**:
- [x] #192 - crt0 runtime startup code ✅
- [x] #193 - Minimal userland libc ✅
- [x] #194 - Syscall ABI documentation ✅
- [x] #195 - Userland build system ✅
- [x] #29 - Cross-compiler toolchain integration ✅
- [ ] #190 - TCC (Tiny C Compiler) port
- [ ] #191 - binutils (as, ld) port
- [ ] #196 - Fish shell research

**Critical Path**: #192 ✅ → #193 ✅ → #194 ✅ → #195 ✅ → #29 ✅ → #190/#191

**Dependencies**:
- #190 requires #29 ✅, #189 (FAT32 writes), **Buddy Allocator milestone** (robust memory for compiler)
- #191 requires #29 ✅, #189 (FAT32 writes), **Buddy Allocator milestone** (robust memory for linker)

**Progress**: Core toolchain complete! crt0, libc, syscall ABI docs, separated build system, and x86_64-elf cross-compiler integration all done. Remaining: TCC/binutils ports for native compilation and Fish shell research.

---

### 4. **Doom** (Game Porting Milestone)
**Goal**: Run Doom (1993) in userland on meniOS
**GitHub Milestone**: [Doom](https://github.com/pbalduino/menios/milestone/3)

**Status**: 10/26 complete (38.5%)

**Note**: Depends on **Buddy Allocator milestone** for efficient memory management under game engine load.

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
- [x] #95 - Userspace memory allocator (malloc/free) — libc arena allocator now exposes SYS_GETPAGESIZE and ships with stress coverage

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
│  - Interactive shell complete ✅          │
│  - Basic utilities working               │
│  - Development environment ready         │
└────────────────┬────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────┐
│  Buddy Allocator (Memory Management)     │
│  - Power-of-2 block allocation           │
│  - Fast split/coalesce operations        │
│  - Bounded fragmentation                 │
│  - Foundation for complex apps           │
└─────┬──────────────────────────┬────────┘
      │                          │
      ↓                          ↓
┌─────────────────────┐   ┌──────────────────────────┐
│  GCC (Toolchain)    │   │  Doom (Full OS Caps)     │
│  - Native compile   │   │  - Graphics, audio       │
│  - Complex tools    │   │  - Threading, IPC        │
│  - Needs robust     │   │  - Game engine stress    │
│    memory mgmt      │   │  - Needs efficient alloc │
└─────────────────────┘   └──────────────────────────┘
```

## 📈 Overall Progress

- **Total Issues Across Milestones**: 70 issues
- **Completed**: 43 issues (61.4%)
- **In Progress**: 27 issues
- **Ready to Start**: 2 issues (no dependencies: #109, #221)
- **Next Up**: #246 (Define Buddy Orders - dependency met!)

## 🚀 Immediate Next Steps

### Ready to Start Now (No Dependencies):
1. **Doom Milestone**:
   - #109 - pthread API
   - #221 - Fast syscall instruction

### Next in Critical Path (Dependencies Met):
1. **Buddy Allocator Milestone**:
   - #246 - Define Buddy Allocator Orders and Configuration (depends on #245 ✅)

## 📝 Notes

### Parallel Development
Many issues can be worked on in parallel:
- **Buddy Allocator**: Linear dependency chain, but #250/#251 can be parallel
- **GCC**: Blocked on Buddy Allocator completion
- **Doom Threading**: #109, #112, #113 are independent (can start now!)
- **Doom IPC**: Different IPC mechanisms can progress in parallel
- **Mosh UX**: All features complete! ✅

### Critical Dependencies
- **Buddy Allocator milestone** is foundational - required by both GCC and Doom
- **GCC milestone** depends on Buddy Allocator for native compilation
- **Doom milestone** depends on Buddy Allocator for game engine memory management
- **Mosh milestone** provides the development environment for debugging ✅

### Completion Order
Recommended completion order for maximum impact:
1. **Mosh** - Provides usable development environment (100% complete) 🎉
2. **Buddy Allocator** - Foundation for complex apps (11.1% complete, in progress!) 🔨
3. **GCC** - Enables native development and compilation (62.5% complete, blocked on Buddy) 🚀
4. **Doom** - Demonstrates full OS capabilities (38.5% complete, some work can start now)

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
- **2025-10-12**: Closed #159 after landing stderr redirection and fd duplication support
- **2025-10-13**: Deferred #202 (/dev/zero EOF bug) out of the Mosh milestone; milestone now sits at 27 completed issues
- **2025-10-14**: Closed #95 (userspace malloc/free) ✅ – libc now ships an arena allocator with regression coverage
- **2025-10-13**: Released v0.1.0 - Mosh milestone complete! 🎉
- **2025-10-13**: Created #234 (Port grep utility)
- **2025-10-13**: Created #235 (Port xargs utility)
- **2025-10-13**: Created #236 (Delete key not working bug)
- **2025-10-13**: Closed #194 (Syscall ABI documentation) ✅ - see `docs/architecture/syscall_abi.md`
- **2025-10-13**: Closed #195 (Userland build system) ✅ - `make userland` now builds libc + /bin independently
- **2025-10-13**: GCC milestone reaches 50% - toolchain foundation complete, ready for #29!
- **2025-10-13**: Closed #29 (Cross-compiler toolchain integration) ✅ - x86_64-elf-gcc now preferred, fallback to host compiler
- **2025-10-13**: GCC milestone reaches 62.5% - core toolchain complete! 🚀
- **2025-10-14**: Created #237 (End-to-end shell validation script)
- **2025-10-14**: Created #238 (Add rand() and time() support) - broken into #239, #240, #241
- **2025-10-14**: Created #239 (Kernel time syscalls - SYS_TIME, SYS_GETTIMEOFDAY)
- **2025-10-14**: Created #240 (Implement time() and gettimeofday() in libc)
- **2025-10-14**: Created #241 (Implement rand() and srand() in libc)
- **2025-10-14**: Issue #202 remains open for further investigation
- **2025-10-12**: Shell polish – `export` persists env entries and `echo` mirrors POSIX quoting rules
- **2025-10-08**: Closed #157 as duplicate of #197, #165 as completed by #208
- **2025-10-14**: Created Buddy Allocator milestone (#245-#253) - 9 issues for first-fit → buddy migration
- **2025-10-14**: Both GCC and Doom milestones now depend on Buddy Allocator completion
- **2025-10-14**: Created #242 (PATH-aware tab completion), #243 (/bin/mem utility), #244 (procfs)
- **2025-10-14**: Total issues across milestones: 70 (was 61)
- **2025-10-14**: Closed #245 (Survey Current Heap) ✅ - Buddy Allocator milestone 11.1% complete!

---

**Last Updated**: 2025-10-14
**See Also**:
- [Road to Shell](road/road_to_shell.md)
- [Road to Buddy Allocator](road/road_to_buddy_allocator.md) 🆕
- [Road to GCC](road/road_to_gcc.md)
- [Road to Doom](road/road_to_doom.md)
