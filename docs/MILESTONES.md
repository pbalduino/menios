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

**Status**: 20/20 complete (100%) 🎉

**Assigned Issues**:

#### Core Implementation (9 issues) ✅ COMPLETE
- [x] #245 - Survey Current Heap Implementation ✅
- [x] #246 - Define Buddy Allocator Orders and Configuration ✅
- [x] #247 - Rewrite Arena Setup for Buddy Allocator ✅
- [x] #248 - Implement Buddy Split and Coalesce Operations ✅
- [x] #249 - Integrate Buddy Allocator with malloc/free ✅
- [x] #250 - Adapt realloc/reallocarray for Buddy Allocator ✅
- [x] #251 - Update Direct mmap Path for Large Allocations ✅
- [x] #252 - Add Buddy Allocator Diagnostics and Tests ✅
- [x] #253 - Cleanup and Document Buddy Allocator Migration ✅

#### Critical Fixes (3 issues) ✅ COMPLETE
- [x] #263 - Kernel heap: stale virtual mappings after region release ✅
- [x] #264 - Kernel heap: partial mapping rollback missing ✅
- [x] #265 - User buddy allocator: thread safety ✅

#### High Priority Fixes (5 issues) ✅ COMPLETE
- [x] #266 - Buddy allocator: improve double-free detection and diagnostics ✅
- [x] #267 - Buddy allocator: missing NULL check in grow_heap ✅
- [x] #268 - Buddy allocator: direct mmap alignment calculation error ✅
- [x] #273 - Buddy allocator: use-after-free risk in buddy_coalesce_block ✅
- [x] #272 - Kernel heap: virtual address exhaustion ✅

#### Performance Improvements (3 issues) ✅ COMPLETE
- [x] #269 - Buddy allocator: O(A×O) linear search across arenas ✅
- [x] #270 - Kernel heap: O(n²) coalescing in kfree ✅
- [x] #271 - Buddy allocator: freelist linear search during coalescing ✅

**Critical Path**: #245 → #246 → #247 → #248 → #249 → #250/#251 → #252 → #253 ✅

**Dependencies**:
- #246 depends on #245 (survey)
- #247 depends on #246 (configuration)
- #248 depends on #247 (arena setup)
- #249 depends on #248 (split/coalesce)
- #250 depends on #249 (malloc/free)
- #251 depends on #249 (malloc/free)
- #252 depends on #248, #249 (core operations + API)
- #253 depends on #252 (testing)

**Progress**: Buddy allocator migration is fully complete. Userland now allocates from 128 MiB arenas managed by per-order freelists, split/coalesce is covered by regression tests, diagnostics expose allocator health via `/proc/meminfo` and `menios_malloc_stats()`, and documentation cleanup is finished. This foundation unblocks the GCC and Doom milestones by delivering predictable heap behaviour under heavy workloads.

**Estimated Effort**: ✅ Finished (actual delivery matched the high-end estimate)

---

### 3. **GCC** (Toolchain Milestone)
**Goal**: Enable native compilation on meniOS with GCC toolchain support
**GitHub Milestone**: [GCC](https://github.com/pbalduino/menios/milestone/2)

**Status**: 5/7 complete (71.4%)

**Assigned Issues**:
- [x] #192 - crt0 runtime startup code ✅
- [x] #193 - Minimal userland libc ✅
- [x] #194 - Syscall ABI documentation ✅
- [x] #195 - Userland build system ✅
- [x] #29 - Cross-compiler toolchain integration ✅
- [ ] #190 - TCC (Tiny C Compiler) port
- [ ] #191 - binutils (as, ld) port

**Critical Path**: #192 ✅ → #193 ✅ → #194 ✅ → #195 ✅ → #29 ✅ → #190/#191

**Dependencies**:
- #190 requires #29 ✅, #189 ✅ (FAT32 writes COMPLETE), **Buddy Allocator milestone** ✅ (COMPLETE)
- #191 requires #29 ✅, #189 ✅ (FAT32 writes COMPLETE), **Buddy Allocator milestone** ✅ (COMPLETE)

**Progress**: Core toolchain complete! crt0, libc, syscall ABI docs, separated build system, and x86_64-elf cross-compiler integration all done. Remaining: TCC/binutils ports for native compilation.

---

### 4. **Doom** (Game Porting Milestone)
**Goal**: Run Doom (1993) in userland on meniOS
**GitHub Milestone**: [Doom](https://github.com/pbalduino/menios/milestone/3)

**Status**: 19/36 complete (52.8%)

**Note**: Depends on **Buddy Allocator milestone** for efficient memory management under game engine load.

**Assigned Issues**:

#### Graphics & Audio (4 issues)
- [x] #31 - Userspace graphics interface ✅
- [x] #32 - Input subsystem ✅
- [ ] #33 - Audio subsystem
- [ ] #143 - Mouse driver

#### libc Gaps for Doom (6 issues)
- [ ] #305 - File stdio support (fopen/fclose, buffered I/O)
- [ ] #306 - Filesystem helpers (mkdir, remove, rename)
- [ ] #307 - Environment variable access (getenv/putenv)
- [ ] #308 - String utilities (strdup, strcasecmp, strncasecmp)
- [ ] #309 - Expose snprintf/vsnprintf/vsprintf properly
- [ ] #310 - Math library (fabs and clean up math.h)

#### Doom Integration (7 issues)
- [x] #300 - Wire up meniOS port layer ✅
- [x] #301 - Expose pixel-addressable framebuffer (/dev/fb0 mmap) ✅
- [x] #302 - Deliver real key events (scan codes, key up/down) ✅
- [ ] #303 - Build integration for Doom userland binary
- [x] #304 - scanf family (sscanf/scanf/fscanf) implementation ✅
- [ ] #311 - Doom meniOS-specific build system (Makefile.menios)
- [ ] #312 - Integrate Doom into meniOS build and packaging

#### Threading Support (5 issues)
- [ ] #109 - pthread API implementation (ready now!)
- [ ] #110 - Thread-safe C library
- [ ] #111 - Advanced pthread synchronization
- [ ] #112 - Thread debugging and profiling
- [ ] #113 - Thread-aware system calls

#### Memory & Process (1 issue)
- [x] #95 - Userspace memory allocator (malloc/free) ✅

#### File System (1 issue)
- [x] #189 - FAT32 write support ✅

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

#### IPC - Other (2 issues)
- [x] #220 - ioctl syscall ✅
- [x] #221 - Fast syscall instruction ✅

**Dependencies**:
- #110 requires #109
- #111 requires #109
- #213 requires #212 ✅ COMPLETE
- #214 requires #213, #109
- #301 requires #31 ✅, #89 ✅, #220 ✅ (framebuffer infrastructure)
- #302 requires #32 ✅, #220 ✅ (input infrastructure) ✅ COMPLETE
- #305 requires #96 ✅, #189 ✅, #294 ✅, #193 ✅ (file stdio)
- #306 requires #193 ✅, #60 ✅, #65 ✅ (filesystem helpers)
- #307 requires #148 ✅, #193 ✅ (environment access)
- #308 requires #95 ✅, #193 ✅ (string utilities)
- #309 requires #193 ✅, #304 ✅ (formatted I/O)
- #310 requires #193 ✅ (math library)
- #311 requires #305, #306, #307, #308, #309, #310, #300 ✅ (Doom build system)
- #312 requires #311, #192 ✅, #193 ✅, #195 ✅, #29 ✅ (build integration)
- #300 requires #301 ✅, #302 ✅, #287 ✅, #240 ✅, #288 ✅, #304 ✅, #305-#310 (libc gaps) ✅ COMPLETE
- #303 requires #312 (old build integration - superseded by #311/#312)
- #304 requires #193 ✅ (libc foundation COMPLETE)

**Progress**: IPC infrastructure complete - Pipes ✅, Signals (5/6), Shared Memory ✅! FAT32 write support ✅ complete! scanf family (#304) ✅ complete! Real key events (#302) ✅ complete! Doom port layer (#300) ✅ complete with fullscreen graphics, timing, and input! Pixel-addressable framebuffer (#301) ✅ complete! New libc gaps identified for Doom (#305-#310). Doom build system planned (#311-#312). Ready to implement libc gaps!

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

- **Total Issues Across Milestones**: 92 issues (includes foundational issues)
- **Completed**: 77 issues (83.7%)
- **In Progress**: 17 issues
- **Ready to Start**: 11 issues (no dependencies: #109, #190, #191, #305-#310)
- **Recently Completed**: #286-#293 ✅ (time management & FAT32 writes), #304 ✅ (scanf family), #302 ✅ (real key events), #301 ✅ (framebuffer mmap), #300 ✅ (menIOS Doom port layer), #136 ✅ (devfs), #221 ✅ (fast syscalls)
- **Recently Created**: #305-#312 ✅ (libc gaps & Doom build system)
- **Next Up**: libc gaps (#305-#310) ready to start! TCC/binutils (#190, #191) ready! Doom build system (#311-#312) waits on libc gaps.

## 🚀 Immediate Next Steps

### 🎉 Buddy Allocator: COMPLETE (100%)!
All phases finished! Core buddy allocator implementation (Phase 1 ✅), critical security issues (Phase 2 ✅), reliability fixes (Phase 3 ✅), and performance optimizations (Phase 4 ✅) are ALL DONE!

**Phase 4 - Performance Optimizations**: ✅ COMPLETE
- ~~#269 - O(A×O) arena linear search optimization~~ ✅ (per-order non-empty arena lists)
- ~~#270 - Kernel O(n²) coalescing fix (doubly-linked lists)~~ ✅
- ~~#271 - Freelist linear search optimization (bitmap/hash table)~~ ✅

**The Buddy Allocator milestone (20/20 issues) is now production-ready and FULLY UNBLOCKS the GCC and Doom milestones!**

### Ready to Start Now (Dependencies Met):
1. **GCC Milestone** (FULLY UNBLOCKED 🚀):
   - #190 - TCC port (Buddy Allocator ✅ COMPLETE, FAT32 writes ✅ COMPLETE)
   - #191 - binutils port (Buddy Allocator ✅ COMPLETE, FAT32 writes ✅ COMPLETE)

2. **Doom Milestone - libc Gaps** (READY NOW! ✅):
   - #305 - File stdio support (all dependencies met)
   - #306 - Filesystem helpers (all dependencies met)
   - #307 - Environment variable access (all dependencies met)
   - #308 - String utilities (all dependencies met)
   - #309 - Expose snprintf/vsnprintf properly (all dependencies met)
   - #310 - Math library (all dependencies met)

3. **Doom Milestone - Threading** (Ready to implement):
   - #109 - pthread API (no dependencies)

4. **Doom Milestone - Integration** (Port layer & framebuffer ✅; build pending):
   - #301 - Pixel-addressable framebuffer ✅ COMPLETE!
   - #302 - Real key events ✅ COMPLETE!
   - #300 - Doom port layer ✅ COMPLETE!
   - #311 - Doom build system (blocked on #305-#310)
   - #312 - Build integration (blocked on #311)

## 📝 Notes

### Parallel Development
Many issues can be worked on in parallel:
- **Buddy Allocator**: ✅ COMPLETE (100%) - All phases done!
- **GCC**: FULLY UNBLOCKED - #190/#191 can start (only needs #189 FAT32 writes)
- **Doom Threading**: #109, #112, #113 are independent (can start now!)
- **Doom IPC**: Different IPC mechanisms can progress in parallel
- **Mosh UX**: All features complete! ✅

### Critical Dependencies
- **Buddy Allocator milestone** ✅ COMPLETE (100%) - Production-ready! All 4 phases complete: Core, Security, Reliability, Performance
- **GCC milestone** - FULLY UNBLOCKED! Only needs #189 (FAT32 writes) for native compilation
- **Doom milestone** - FULLY UNBLOCKED! Buddy allocator complete, ready for pthread (#109) and advanced features
- **Mosh milestone** ✅ COMPLETE - Development environment ready!

### Completion Order
Recommended completion order for maximum impact:
1. **Mosh** ✅ COMPLETE (100%) - Development environment ready! 🎉
2. **Buddy Allocator** ✅ COMPLETE (100%) - Memory management production-ready! 🎉
3. **GCC** - Enables native development (62.5% complete, FULLY UNBLOCKED, only needs #189)
4. **Doom** - Demonstrates full OS capabilities (38.5% complete, FULLY UNBLOCKED, ready to start!)

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
- **2025-10-12**: Closed #221 (fast syscall/sysret path) ✅
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
- **2025-10-14**: Closed #246 (Define Buddy Orders and Configuration) ✅ - Buddy Allocator milestone 22.2% complete!
  - Orders 7–27 (128 B–128 MiB), 128 MiB arenas, 21 freelists
- **2025-10-14**: Closed #247 (Rewrite Arena Setup) ✅ - Buddy Allocator milestone 33.3% complete!
  - New arenas seed order-27 root blocks with per-order freelists
- **2025-10-14**: Closed #248 (Buddy Split/Coalesce) ✅ - Buddy Allocator milestone 44.4% complete!
  - Core buddy algorithms implemented with host regression tests
- **2025-10-14**: Closed #249 (malloc/free Integration) ✅ - Buddy Allocator milestone 55.6% complete!
  - malloc() and free() now route through buddy allocator with order-based allocation
- **2025-10-14**: Closed #250 (realloc Adaptation) ✅ - Buddy Allocator milestone 66.7% complete!
  - realloc() attempts in-place expansion via buddy merging, splits on shrinking
- **2025-10-14**: Closed #251 (Direct mmap Path) ✅ - Buddy Allocator milestone 77.8% complete!
  - Handles large allocations and unusual alignments (posix_memalign, memalign, valloc, pvalloc)
- **2025-10-14**: Closed #252 (Buddy Allocator Diagnostics and Tests) ✅ - Buddy Allocator milestone 88.9% complete!
  - Added `menios_malloc_stats()` and comprehensive regression tests
- **2025-10-14**: Closed #253 (Cleanup and Documentation) ✅ - Buddy Allocator Phase 1 complete!
  - Legacy first-fit code removed, buddy design fully documented
  - Core implementation finished, moving to reliability and performance phases
- **2025-10-14**: Completed memory allocation analysis - identified 23 issues across security, performance, and design
  - Created `docs/memory_issues.md` with comprehensive analysis
  - Closed #263 (kernel stale mappings) ✅, #264 (partial rollback) ✅, #265 (thread safety) ✅
- **2025-10-14**: Created 8 new Buddy Allocator issues (#266-#273) for reliability and performance
  - High priority: ~~#266 (double-free detection)~~, #267 (NULL check), ~~#272 (VA exhaustion)~~, ~~#273 (use-after-free)~~, ~~#268 (alignment)~~
  - Performance: #269 (arena search), #270 (kernel coalescing), #271 (freelist search)
  - Buddy Allocator milestone expanded to 20 issues (was 9)
  - Total project issues: 81 (was 70)
- **2025-10-14**: Closed #267 (grow_heap NULL check) ✅ - Buddy Allocator 65% complete (13/20)
  - Arena is now properly unlinked before cleanup if buddy_materialize_block fails
  - Prevents dangling pointer crashes on subsequent allocations
- **2025-10-14**: Closed ALL remaining Buddy Allocator issues! 🎉
  - Closed #268 (alignment calculation) ✅, #266 (double-free detection) ✅
  - Closed #269 (arena search optimization) ✅, #270 (kernel coalescing) ✅, #271 (freelist optimization) ✅
  - Closed #272 (VA exhaustion) ✅, #273 (use-after-free in coalesce) ✅
  - **Buddy Allocator milestone reaches 100% (20/20) - PRODUCTION READY!** 🎉
  - All phases complete: Core (9/9) ✅, Critical Security (3/3) ✅, Reliability (5/5) ✅, Performance (3/3) ✅
  - GCC and Doom milestones now FULLY UNBLOCKED!
- **2025-10-16**: Closed #238 (rand() and time() support) ✅
  - Closed #239 (Kernel time syscalls: SYS_TIME, SYS_GETTIMEOFDAY) ✅
  - Closed #240 (time() and gettimeofday() in userland libc) ✅
  - Closed #241 (rand() and srand() in userland libc) ✅
  - Implemented thread-safe PRNG with LCG algorithm in libc
  - Added SYS_TIME (81) and SYS_GETTIMEOFDAY (82) syscalls
  - Both time functions return proper Unix timestamps
- **2025-10-17**: Closed #274 (Boot regression: syscall return path truncation) ✅
  - Fixed syscall/sysret return path to preserve full 64-bit values
  - mmap now correctly returns 64-bit pointers to userland
  - Boot regression resolved - system boots successfully
  - Issue #221 (fast syscalls) now fully complete with proper 64-bit ABI
- **2025-10-17**: Closed #291 (Expose FAT32 create/truncate primitives) ✅
  - Part of #189 (FAT32 write support) - Phase 1 complete
  - Added exported wrappers: `fs_path_create_file()` and `fs_file_truncate()`
  - Exposed existing FAT32 internal functions to VFS layer
  - Foundation in place for O_CREAT/O_TRUNC support (#292)
- **2025-10-17**: Closed #292 (VFS O_CREAT/O_TRUNC/O_EXCL support) ✅
  - Part of #189 (FAT32 write support) - Phase 2 complete
  - VFS now honors O_CREAT flag for file creation
  - VFS now honors O_TRUNC flag for truncating existing files
  - VFS now honors O_EXCL flag for atomic file creation
  - Removed `-ENOSYS` guard for write operations
  - Userland programs can now create and overwrite files via `open()`
- **2025-10-17**: Closed #293 (FAT32 regression tests) ✅
  - Part of #189 (FAT32 write support) - Phase 3 complete
  - **Issue #189 (FAT32 write support) now 100% COMPLETE!** 🎉
  - Added comprehensive regression tests for file creation and truncation
  - Validated O_CREAT, O_TRUNC, and O_EXCL behavior
  - Confirmed data persistence across writes
  - TCC (#190) and binutils (#191) now FULLY UNBLOCKED!
- **2025-10-17**: Closed #294 (VFS: Streaming I/O and buffer cache - parent) ✅
  - **All 4 phases complete!** 🎉
  - Replaced slurp-and-flush model with streaming block-by-block I/O
  - Implemented LRU block cache with bread()/bwrite() primitives
  - FAT32 refactored to use block cache for all disk operations
  - Added read-ahead and write-behind optimizations
  - Fixed-size cache (4 MiB) handles arbitrarily large files
  - Foundation ready for ext2 filesystem (#60)
- **2025-10-17**: Closed #295 (Block cache infrastructure) ✅
  - Part of #294 - Phase 1 complete
  - Implemented buffer_head structure with LRU eviction
  - Hash table for O(1) block lookup
  - bread() and bwrite() primitives for block I/O
  - Cache eviction policy handles memory pressure
- **2025-10-17**: Closed #296 (FAT32 block-level I/O) ✅
  - Part of #294 - Phase 2 complete
  - Refactored FAT32 to use bread()/bwrite() instead of direct ATA reads
  - FAT table and directory entries now cached
  - Reduced memory footprint - no need to load entire FAT table
- **2025-10-17**: Closed #297 (VFS streaming operations) ✅
  - Part of #294 - Phase 3 complete
  - Replaced vfs_read() slurp with block-by-block streaming
  - Replaced vfs_write() flush-on-close with incremental writes
  - Implemented lseek() for arbitrary file positioning
  - Removed per-file memory buffers
- **2025-10-17**: Closed #298 (Read-ahead and write-behind) ✅
  - Part of #294 - Phase 4 complete
  - Sequential access detection with 4-block prefetch
  - Async write-behind with dirty page tracking
  - Periodic flusher thread (5 second interval)
  - Added sync() and fsync() syscalls for explicit durability
- **2025-10-17**: Closed #290 (Time conversion utilities - gmtime/mktime/strftime) ✅
  - Implemented gmtime_r() for UTC time breakdown
  - Implemented mktime() for struct tm to time_t conversion
  - Implemented strftime() with comprehensive format specifiers
  - Added timezone abbreviation support (UTC, GMT, etc.)
  - Foundation in place for full timezone support (#299)
- **2025-10-17**: Closed #287 (nanosleep() and sleep queue) ✅
  - Part of #226 (RTC and time management) breakdown
  - Implemented SYS_NANOSLEEP syscall with sleep queue
  - Processes can sleep for specified durations with nanosecond precision
  - Sleep queue properly handles timer interrupts and process wakeup
  - Foundation for alarm(), usleep(), and other timing primitives
- **2025-10-17**: Closed #286 (Kernel RTC driver and time management) ✅
  - Part of #226 (RTC and time management) breakdown
  - Implemented RTC (Real-Time Clock) driver for CMOS hardware access
  - System time initialization from hardware clock at boot
  - Kernel maintains accurate system time via timer interrupts
  - Foundation for wall-clock time syscalls and scheduling
- **2025-10-17**: Closed #221 (Fast syscall instruction - syscall/sysret) ✅
  - Implemented fast syscall/sysret path for x86-64
  - Proper 64-bit return value handling (#274 fixed)
  - Significant performance improvement over legacy int 0x80
  - All syscalls now use SYSCALL instruction
  - Foundation for high-performance system calls
- **2025-10-17**: Closed #288 (Interval timers - setitimer) ✅
  - Part of #226 (RTC and time management) breakdown
  - Implemented setitimer() syscall for ITIMER_REAL, ITIMER_VIRTUAL, ITIMER_PROF
  - Interval timers deliver SIGALRM on expiration
  - Enables timeout mechanisms and periodic signal delivery
  - Foundation for alarm(), ualarm(), and timer-based operations
- **2025-10-17**: Closed #289 (alarm() syscall) ✅
  - Implemented SYS_ALARM wrapper over the ITIMER_REAL engine
  - Returns remaining seconds from prior alarms with POSIX rounding semantics
  - libc exposes alarm() with host fallback for MENIOS_HOST_TEST builds
  - Regression coverage added in `test/test_syscall_alarm.c`
- **2025-10-17**: Removed #105, #106, #107 from Doom milestone
  - Unix domain sockets, microkernel IPC, and capability-based security moved out
  - These are advanced IPC features not required for Doom
  - Doom milestone now focuses on core game requirements
- **2025-10-17**: Created #304 (scanf family implementation - sscanf/scanf/fscanf)
  - Added to GCC milestone
  - Formatted input parsing for config files (Doom requirement)
  - Dependencies: #193 ✅ (libc foundation COMPLETE)
  - Required for #300 (Doom port layer - config file parsing) ✅
  - Total issues across milestones: 83 (was 82)
- **2025-10-18**: Closed #304 (scanf family - sscanf/scanf/fscanf) ✅
  - Moved from GCC to Doom milestone
  - Complete scanf family implementation with C99 format support
  - Doom config parsing now functional
  - GCC milestone: 5/7 complete (71.4%)
  - Doom milestone: 15/28 complete (53.6%)
  - Overall progress: 72/83 issues complete (86.7%)
- **2025-10-18**: Created 8 new Doom issues (#305-#312) for remaining libc gaps and build integration
  - libc gaps: #305 (file stdio), #306 (filesystem helpers), #307 (environment access), #308 (string utilities), #309 (formatted I/O), #310 (math library)
  - Build system: #311 (Doom Makefile.menios), #312 (build integration)
  - All libc gap issues (#305-#310) ready to start immediately - all dependencies met!
  - Doom milestone: 15/36 complete (41.7%)
  - Total project issues: 91 (was 83)
  - Overall progress: 72/91 issues complete (79.1%)
- **2025-10-19**: Closed #302 (Real key events - scan codes, key up/down) ✅
  - PS/2 driver now reports both key press and release events
  - Scan codes preserved and accessible
  - Extended scan codes (0xE0 prefix) handled correctly
  - Doom input handling unblocked
  - Doom milestone: 16/36 complete (44.4%)
  - Overall progress: 73/91 issues complete (80.2%)
- **2025-10-19**: Closed #300 (Doom port layer) ✅
  - DG_Init/DG_DrawFrame map `/dev/fb0`, acquire ownership, and flush via framebuffer ioctls
  - DG_GetKey(), DG_SleepMs(), DG_GetTicksMs() wired through menios_input_poll(), nanosleep(), and clock_gettime()
  - Doomgeneric shutdown path restores framebuffer mode and releases ownership
- **2025-10-19**: Closed #136 (Device filesystem infrastructure) ✅
  - devfs driver fully implemented in src/kernel/fs/devfs.c
  - /dev directory mounted at boot via devfs_mount()
  - Standard device nodes available: /dev/null, /dev/zero, /dev/tty0, /dev/console, /dev/ttyS0
  - Device file operations (open/close/read/write) route correctly to handlers
  - VFS integration complete
  - Foundational infrastructure - has been in production use
  - Overall progress: 74/92 issues complete (80.4%)
- **2025-10-19**: Closed #300 (Doom port layer) ✅, #301 (Framebuffer mmap) ✅
  - Full doomgeneric port layer implementation complete
  - /dev/fb0 framebuffer access with ownership model and ioctl suite
  - Double buffering, mode management, graceful shutdown all working
  - Doom milestone: 19/36 complete (52.8%)
  - Overall progress: 77/92 issues complete (83.7%)

---

**Last Updated**: 2025-10-19
**See Also**:
- [Road to Shell](road/road_to_shell.md)
- [Road to Buddy Allocator](road/road_to_buddy_allocator.md)
- [Road to GCC](road/road_to_gcc.md)
- [Road to Doom](road/road_to_doom.md)
- [Road to Real Hardware](road/road_to_real_hardware.md) 🆕
