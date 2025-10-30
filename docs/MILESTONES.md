# meniOS Milestones

This document tracks the major milestones for meniOS development.

> **Release v0.1.666 "DOOM READY" (2025-10-20):** Completes the Doom milestone with all infrastructure for running the classic 1993 Doom game in userland. This release achieves 97.8% overall completion (87/89 issues) with three milestones at 100%.

> **Release v0.1.0 "SHELL READY" (2025-10-13):** First public release celebrating the completion of the Mosh shell milestone with a polished interactive shell experience.

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

**Status**: 6/7 complete (85.7%)

**Assigned Issues**:
- [x] #192 - crt0 runtime startup code ✅
- [x] #193 - Minimal userland libc ✅
- [x] #194 - Syscall ABI documentation ✅
- [x] #195 - Userland build system ✅
- [x] #29 - Cross-compiler toolchain integration ✅
- [x] #191 - binutils port (as, ld, ar, ranlib, nm, objdump, readelf, size, objcopy, strip, strings) ✅ **CLOSED 2025-10-29**
- [ ] #190 - TCC (Tiny C Compiler) port

**Critical Path**: #192 ✅ → #193 ✅ → #194 ✅ → #195 ✅ → #29 ✅ → #191 ✅ → #190

**Dependencies**:
- #190 requires #29 ✅, #189 ✅ (FAT32 writes COMPLETE), **Buddy Allocator milestone** ✅ (COMPLETE), **#191 ✅** (binutils COMPLETE)
- ~~#191 requires #29 ✅, #189 ✅ (FAT32 writes COMPLETE), **Buddy Allocator milestone** ✅ (COMPLETE)~~ ✅ COMPLETE

**Progress**: Core toolchain + binutils complete! Full GNU binutils 2.45 suite operational on meniOS with all 11 tools working (as, ld, ar, ranlib, nm, objdump, readelf, size, objcopy, strip, strings). Complete native development environment achieved. Remaining: TCC port for native compilation.

**Achievement**: Complete GNU binutils 2.45 toolchain now available! Developers can assemble, link, create static libraries, and analyze binaries entirely within meniOS. This represents a major milestone in native development capabilities.

---

### 4. **Doom** (Game Porting Milestone) ✅ **COMPLETE**
**Goal**: Run Doom (1993) in userland on meniOS
**GitHub Milestone**: [Doom](https://github.com/pbalduino/menios/milestone/3)

**Status**: ✅ 33/33 complete (100%) - **Released in v0.1.666 "DOOM READY"**

**Achievement**: All required infrastructure for running Doom in userland is complete. The milestone includes 33 essential issues covering graphics, input, libc gaps, build integration, stability fixes, IPC mechanisms, and core infrastructure.

**Assigned Issues**:

#### Graphics & Input (2 issues) ✅ **ALL COMPLETE**
- [x] #31 - Userspace graphics interface ✅
- [x] #32 - Input subsystem ✅

#### libc Gaps for Doom (6 issues) ✅ **ALL COMPLETE!**
- [x] #305 - File stdio support (fopen/fclose, buffered I/O) ✅
- [x] #306 - Filesystem helpers (mkdir, remove, rename) ✅
- [x] #307 - Environment variable access (getenv/putenv) ✅
- [x] #308 - String utilities (strdup, strcasecmp, strncasecmp) ✅
- [x] #309 - Expose snprintf/vsnprintf/vsprintf properly ✅
- [x] #310 - Math library (fabs and clean up math.h) ✅

#### Doom Integration (7 issues) ✅ **ALL COMPLETE**
- [x] #300 - Wire up meniOS port layer ✅
- [x] #301 - Expose pixel-addressable framebuffer (/dev/fb0 mmap) ✅
- [x] #302 - Deliver real key events (scan codes, key up/down) ✅
- [x] #303 - Build integration for Doom userland binary ✅ (superseded by #311/#312)
- [x] #304 - scanf family (sscanf/scanf/fscanf) implementation ✅
- [x] #311 - Doom meniOS-specific build system (Makefile.menios) ✅
- [x] #312 - Integrate Doom into meniOS build and packaging ✅

#### Stability (6 issues) ✅ **ALL COMPLETE**
- [x] #262 - User-mode page fault handling ✅
- [x] #274 - Init process crash fix ✅
- [x] #319 - Doom null pointer dereference crash ✅
- [x] #320 - Shell hang after Doom crash ✅
- [x] #322 - Shell alarm crash (stack overflow) ✅
- [x] #323 - Shell crash after Doom exit ✅

#### Memory & Process (1 issue) ✅
- [x] #95 - Userspace memory allocator (malloc/free) ✅

#### IPC - Signals (4 issues) ✅ **ALL COMPLETE!**
- [x] #103 - UNIX signals (parent issue) ✅
- [x] #211 - Signal syscalls ✅
- [x] #212 - Signal delivery path ✅
- [x] #213 - Shell Ctrl+C integration ✅

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
- #213 requires #212 ✅ COMPLETE
- #301 requires #31 ✅, #89 ✅, #220 ✅ (framebuffer infrastructure)
- #302 requires #32 ✅, #220 ✅ (input infrastructure) ✅ COMPLETE
- #305 requires #96 ✅, #189 ✅, #294 ✅, #193 ✅ (file stdio)
- #306 requires #193 ✅, #60 ✅, #65 ✅ (filesystem helpers)
- #307 requires #148 ✅, #193 ✅ (environment access)
- #308 requires #95 ✅, #193 ✅ (string utilities) ✅ COMPLETE
- #309 requires #193 ✅, #304 ✅ (formatted I/O)
- #310 requires #193 ✅ (math library)
- #311 requires #305, #306, #307, #308, #309, #310, #300 ✅ (Doom build system) ✅ COMPLETE
- #312 requires #311, #192 ✅, #193 ✅, #195 ✅, #29 ✅ (build integration)
- #300 requires #301 ✅, #302 ✅, #287 ✅, #240 ✅, #288 ✅, #304 ✅, #305-#310 (libc gaps) ✅ COMPLETE
- #303 requires #312 (old build integration - superseded by #311/#312)
- #304 requires #193 ✅ (libc foundation COMPLETE)

**Progress**: 🎉 **MILESTONE COMPLETE!** All 33 required issues closed and shipped in v0.1.666 "DOOM READY":
- ✅ Graphics & Input (2/2): Complete framebuffer pipeline with mmap support and real keyboard events
- ✅ libc Gaps (6/6): File stdio, filesystem helpers, environment access, string utilities, formatted I/O, math library
- ✅ Doom Integration (7/7): Complete [doomgeneric](https://github.com/ozkl/doomgeneric) port layer, build system, and packaging integration - Doom ships automatically
- ✅ Stability (6/6): All crash scenarios fixed - null pointer dereference, shell hang/crash after Doom exit, alarm stack overflow
- ✅ Memory & Process (1/1): Userspace malloc backed by production-ready buddy allocator
- ✅ IPC - Signals (4/4): UNIX signals, syscalls, delivery path, shell Ctrl+C integration
- ✅ IPC - Shared Memory (5/5): Manager, syscalls, reference counting, tests, documentation
- ✅ IPC - Other (2/2): ioctl syscall, fast syscall instruction

**Streamlined**: Removed non-essential features (audio, threading, mouse, advanced signals) to future work - initial Doom port doesn't require them.

---

### 6. **GUI** (Graphical User Interface Milestone)
**Goal**: Complete graphical user interface stack with Cairo graphics, Wayland-style compositor, window manager, and desktop environment
**GitHub Milestone**: [GUI](https://github.com/pbalduino/menios/milestone/6)

**Status**: 0/20 complete (0%)

**Timeline**: 32-42 weeks (8-10 months)

#### Milestone Structure

**Phase 1: Graphics Foundation** (8-10 weeks)
- [ ] #397 - Port Pixman (pixel manipulation library, Cairo dependency)
- [ ] #398 - Port FreeType (font rendering library, Cairo dependency)
- [ ] #396 - Port Cairo (2D graphics library, core rendering engine)

**Phase 2: Input Stack** (4-5 weeks)
- [ ] #399 - Implement GUI input event system
- [ ] #400 - Implement mouse cursor rendering
- Blocked on: #143 - PS/2 Mouse Driver (CRITICAL BLOCKER)

**Phase 3: Shared Memory & IPC** (3-4 weeks)
- [ ] #401 - Implement shared memory (POSIX shm) for GUI
- [ ] #402 - Implement Unix domain sockets (AF_UNIX) for IPC

**Phase 4: Compositor Core** (6-8 weeks)
- [ ] #403 - Implement compositor core
- [ ] #404 - Define window protocol for client-compositor communication
- [ ] #405 - Implement composition engine with damage tracking

**Phase 5: Window Manager** (5-7 weeks)
- [ ] #406 - Implement window manager core
- [ ] #407 - Implement focus management for windows
- [ ] #408 - Implement window decorations (title bar, borders, buttons)

**Phase 6: Desktop Environment** (6-8 weeks)
- [ ] #409 - Implement desktop shell (panel, wallpaper, launcher)
- [ ] #410 - Develop core GUI applications (terminal, editor, file manager)

#### Related Issues (Supporting Infrastructure)
- [ ] #143 - PS/2 Mouse Driver (CRITICAL - blocks input stack)
- [ ] #129 - Unicode text rendering (complements Cairo text)
- [ ] #109 - pthread API (needed for compositor threading)
- [ ] #339 - Thread-safe libc (needed for multi-threaded apps)
- [ ] #394 - Terminal scrollback with mouse wheel (will use in GUI terminal)

**Architecture**: Wayland-style compositor where the display server and compositor are unified. Cairo provides 2D graphics rendering, Pixman handles low-level pixel manipulation, and FreeType enables high-quality font rendering.

**Key Features**:
- Vector graphics and anti-aliased rendering
- High-quality text with TrueType fonts
- Window management (move, resize, minimize, maximize, close)
- Desktop shell with panel/taskbar and application launcher
- Core GUI applications (terminal emulator, text editor, file manager)
- Foundation for future 3D graphics (OpenGL/Vulkan)

**Dependencies**:
- #143 (PS/2 Mouse) - **CRITICAL BLOCKER** for input stack
- #109 (pthread API) - Required for compositor threading
- #339 (Thread-safe libc) - Required for multi-threaded GUI apps
- #193 (libc) - Foundation for all userland code

**Documentation**: See [docs/road/road_to_gui.md](road/road_to_gui.md) for complete GUI roadmap with implementation details, architecture diagrams, and code examples.

**Success Criteria**:
- Cairo renders to framebuffer with 60fps
- Mouse and keyboard input working in GUI
- Multiple windows can be displayed and managed
- Can move, resize, and close windows
- At least 3 GUI applications working (terminal, editor, file manager)
- Professional desktop appearance

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

- **Total Issues Across Milestones**: 126 issues (includes foundational issues)
- **Completed**: 98 issues (77.8%)
- **In Progress**: 8 issues
- **GUI Milestone**: 0/20 issues (20 new GUI issues created)
- **Ready to Start**: 2 issues (no dependencies: #190, #191)
- **Recently Completed**: #191 ✅ (binutils port - ALL 11 TOOLS WORKING!), #365-#368 ✅ (filesystem metadata + pathconf COMPLETE), #317 ✅ (utime), #369 ✅ (system()), #370 ✅ (stat command), #372 ✅ (shutdown command), #373 ✅ (realpath command), #374 ✅ (head command), #376 ✅ (printf dynamic width), #387 ✅ (PCI device boot listing), #135 ✅ (Gcov integration), #236 ✅ (delete key), #350 ✅ (time command), #77 ✅ (tools docs)
- **Recently Created**: #382-#386 (audio subsystem - 5 issues), #394 (terminal scrollback with mouse wheel), #387 ✅ (PCI boot listing - COMPLETE), #396-#410 (GUI stack - 15 issues), #411 (source file headers)
- **Next Up**: 🎉 Doom milestone COMPLETE (33/33)! GCC milestone 85.7% (6/7) - only TCC remaining! 🎨 **GUI Milestone launched** - 20 issues created for complete graphical desktop environment! Audio subsystem fully planned (5 issues). PCI diagnostics complete with color-coded driver detection.

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

2. **Doom Milestone** (33/33 COMPLETE! 🎉):
   - ✅ Graphics & Input (2/2 complete)
   - ✅ libc Gaps (6/6 complete)
   - ✅ Doom Integration (7/7 complete)
   - ✅ Stability (6/6 complete)
   - ✅ Memory & Process (1/1 complete)
   - ✅ IPC - Signals (4/4 complete)
   - ✅ IPC - Shared Memory (5/5 complete)
   - ✅ IPC - Other (2/2 complete)
   - **ALL ISSUES COMPLETE - Doom runs in userland!** 🎮

## 📝 Notes

### Parallel Development
Completed milestones:
- **Mosh**: ✅ COMPLETE (27/27 - 100%)
- **Buddy Allocator**: ✅ COMPLETE (20/20 - 100%)
- **Doom**: ✅ COMPLETE (33/33 - 100%)

Active work:
- **GCC**: 6/7 complete (85.7%) - Only #190 (TCC) remaining, binutils complete!

### Critical Dependencies
- **Mosh milestone** ✅ COMPLETE (27/27 - 100%)
- **Buddy Allocator milestone** ✅ COMPLETE (20/20 - 100%)
- **Doom milestone** ✅ COMPLETE (33/33 - 100%)
- **GCC milestone** - 6/7 complete (85.7%) - Only #190 (TCC) remaining, #191 (binutils) complete ✅

### Completion Order
1. **Mosh** ✅ COMPLETE (27/27 - 100%) 🎉
2. **Buddy Allocator** ✅ COMPLETE (20/20 - 100%) 🎉
3. **Doom** ✅ COMPLETE (33/33 - 100%) 🎉
4. **GCC** - In Progress (6/7 - 85.7%) - Enables native development, binutils complete!

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
- **2025-10-19**: Closed #300 (Doom port layer) ✅, #301 (Framebuffer mmap) ✅, #311 (Doom build system) ✅
  - Full [doomgeneric](https://github.com/ozkl/doomgeneric) port layer implementation complete
  - /dev/fb0 framebuffer access with ownership model and ioctl suite
  - Double buffering, mode management, graceful shutdown all working
  - Doom build system now compiles [doomgeneric](https://github.com/ozkl/doomgeneric) against meniOS SDK
  - Doom milestone: 19/36 complete (52.8%)
  - Overall progress: 77/92 issues complete (83.7%)
- **2025-10-19**: Closed #140 (/dev/kbd0 and /dev/fb0 device interfaces) ✅
  - Device file interfaces production-ready and used by Doom port
  - /dev/fb0 framebuffer device with complete ioctl suite and mmap support
  - /dev/kbd0 keyboard device with real key events via menios_input_poll
  - Doom milestone: 20/36 complete (55.6%)
  - Overall progress: 78/92 issues complete (84.8%)
- **2025-10-19**: Closed #308 (String utilities) ✅
  - Implemented strdup, strcasecmp, strncasecmp, and other string helpers
  - All string utilities needed for Doom compilation now available
  - Doom milestone: 21/36 complete (58.3%)
  - Overall progress: 79/92 issues complete (85.9%)
- **2025-10-19**: Created #313 (Filesystem hierarchy organization)
  - Proposal to organize /bin, /sbin, /usr/bin following Unix FHS conventions
  - Foundation for better scalability and package management
- **2025-10-19**: Created #314 (Shell startup script support - .moshrc)
  - Automated command execution on shell startup
  - High usability impact for debugging and testing
- **2025-10-19**: Created #315 (Message of the day - motd)
  - Display random welcome messages from /etc/motd
  - Suppressible via ~/.hushlogin (OpenBSD-style)
  - Enhances user experience and system personality
- **2025-10-19**: Created #316 (touch command)
  - File creation and timestamp updates
  - Essential Unix utility for build systems and scripting
- **2025-10-19**: Created #317 (utime() syscall)
  - File timestamp modification syscall
  - Required for touch command and backup/restore tools
  - POSIX compliance for file metadata control
- **2025-10-19**: Created #318 (uname command)
  - System information display (kernel name, version, architecture)
  - Platform detection for scripts and build configuration
  - Total project issues: 98 (was 92)
- **2025-10-19**: Closed #307 (Environment variable access) ✅
  - Implemented getenv() and putenv() for environment variable manipulation
  - Required for Doom configuration and Unix-style environment handling
  - Doom milestone: 22/36 complete (61.1%)
- **2025-10-19**: Closed #309 (Expose snprintf/vsnprintf/vsprintf properly) ✅
  - Proper exposure and testing of formatted output functions
  - Required for Doom string formatting
  - Doom milestone: 23/36 complete (63.9%)
  - Overall progress: 81/98 issues complete (82.7%)
- **2025-10-19**: Closed #305 (File stdio support - fopen/fclose, buffered I/O) ✅
  - Implemented FILE structure with buffering, position tracking, and file descriptor
  - Added fopen/fclose for file opening with mode strings ("r", "w", "a", etc.)
  - Implemented fread/fwrite for buffered I/O operations
  - Added fseek/ftell/rewind for file positioning
  - Implemented fflush for explicit buffer writes
  - Doom milestone: 24/36 complete (66.7%)
- **2025-10-19**: Closed #306 (Filesystem helpers - mkdir, remove, rename) ✅
  - Implemented mkdir() for directory creation
  - Added remove() for file deletion
  - Implemented rename() for file moves
  - Added unlink()/rmdir() wrappers
  - Doom save game management now functional
  - Doom milestone: 25/36 complete (69.4%)
  - Overall progress: 83/98 issues complete (84.7%)
- **2025-10-19**: Closed #310 (Math library - fabs and clean up math.h) ✅
  - Implemented fabs() for double precision floating-point absolute value
  - Implemented fabsf() for single precision (float) version
  - Implemented fabsl() for long double version
  - Cleaned up <math.h> header with proper declarations
  - Uses compiler builtins for optimal performance
  - Doom video scaling calculations (v_video.c) now functional
  - 🎉 **ALL 6 LIBC GAPS NOW COMPLETE!**
  - Doom milestone: 26/36 complete (72.2%)
  - Overall progress: 84/98 issues complete (85.7%)
- **2025-10-19**: Closed #103 (UNIX signals parent issue) ✅
  - All signal subsystem work complete (5/5 issues)
  - IPC - Signals section now 100% complete
  - Removed #111 (Advanced pthread synchronization) from Doom milestone - not required for Doom
  - Removed #112 (Thread debugging and profiling) from Doom milestone - not required for Doom
  - Removed #214 (Advanced signal features) from Doom milestone - not required for Doom
  - Threading Support section reduced from 5 to 3 issues
  - IPC - Signals section reduced from 6 to 5 issues (all complete)
  - Doom milestone reduced from 36 to 32 issues
  - Doom milestone: 27/32 complete (84.4%)
  - Total project issues reduced from 98 to 94
  - Overall progress: 84/94 issues complete (89.4%)
- **2025-10-19**: Closed #314 (Shell startup script support - .moshrc) ✅
  - Implemented .moshrc shell startup script support
  - Shell now executes commands from ~/.moshrc on startup
  - Enables automated environment setup and debugging workflows
  - Enhances developer experience with customizable shell initialization
- **2025-10-19**: Created #319-#322 - **CRITICAL BUGS IDENTIFIED** ⚠️
  - **#319** - Doom crashes with null pointer dereference at 0xf0 during gameplay ✅ **FIXED**
    - Labels: bug, doom, kernel
    - Milestone: Doom
    - Priority: HIGH - Game stability blocker
    - **Root cause**: Race condition - early Up arrow keypress before player->mo initialized
    - **Fix**: Keyboard input gating in vendor/genericdoom/i_input.c:280 - defers input until player ready
    - **Testing**: No crashes with early keypresses, logs confirm gating behavior
    - **Status**: CLOSED ✅
  - **#320** - Shell becomes unresponsive after Doom crashes
    - Labels: bug, doom, mosh, blocked
    - Milestone: Doom
    - Priority: CRITICAL - System completely unusable after crash
    - Depends on: #319 (root cause)
    - **Related to #323** - Likely same root cause (shell fails after Doom exits)
  - **#321** - Page fault handler prints duplicate error messages
    - Labels: bug, kernel
    - Priority: **HIGH** (upgraded from MEDIUM)
    - **UPDATE**: Evidence reveals kernel page fault loop, not just cosmetic issue
    - Kernel fault at 0xffffffff80032301 during sys_exit memcpy from invalid address 0xff300
    - **Directly related to #322** - confirms syscall_frame_override corruption
  - **#322** - mosh crashes with page fault when alarm triggers (stack overflow suspected)
    - Labels: bug, mosh, kernel, blocked
    - Milestone: Doom
    - Priority: CRITICAL - Shell never starts, system loops
    - **Root cause identified**: sys_exit copying from invalid frame pointer 0xff300
    - **Evidence from #321**: Kernel page fault during memcpy, system halt
    - **Reverted fix**: syscall_frame_override path removed (commit 3e77e9f)
    - **Remaining work**: Fix frame pointer corruption in signal/alarm delivery path
  - **#320 + #323** - Shell failure after Doom exits (likely same root cause)
    - #320: Shell **hangs** when Doom crashes
    - #323: Shell **crashes** when Doom exits normally
    - Theory: Same bug in process cleanup, resource release, or signal handling
    - Different symptoms due to timing/exit path differences
    - Should investigate together - fixing one likely fixes both
- **2025-10-19**: Closed #319 (Doom null pointer crash) ✅ **FIXED**
  - **Root cause**: Race condition between keyboard input and player initialization
  - **Fix implemented**: Keyboard input gating in vendor/genericdoom/i_input.c:280
    - Input deferred until players[consoleplayer].mo exists
    - Logs show "delaying keyboard input" → "resuming keyboard input"
  - **Enhanced diagnostics**:
    - Diagnostic breadcrumb retained in vendor/genericdoom/p_user.c:232
    - Kernel page-fault dump expanded (src/kernel/idt.c:190) with full user registers
  - **Testing**: No crashes with early keypresses, gating behavior confirmed
  - **Impact**: Doom no longer crashes from early keyboard input
  - First critical Doom bug resolved! 🎉
- **2025-10-19**: Created #324 (printf field width/padding not implemented)
  - Labels: libc, enhancement, nice to have
  - **Issue**: printf family doesn't honor field widths (%5d) or leading-zero flags (%05d)
  - **Example**: sprintf(buf, "CWILV%2.2d", 0) produces "CWILV0" instead of "CWILV00"
  - **Workaround**: Doom's W_GetNumForName has padded fallback for lump lookup
  - **Impact**: LOW currently (workaround exists), MEDIUM-HIGH for future POSIX code
  - **Fix needed**: Enhance vsnprintf.c to parse and apply width/precision/flags
  - Not blocking Doom, but needed for proper libc compliance
- **2025-10-20**: Created #325-#330 - **RTC and Time Management Breakdown** 🕐
  - **#325** - RTC boot-time synchronization
    - Boot-time sync path with RTC fallback
    - Write path for clock_settime() persistence
    - Dependencies: #286 ✅, #239 ✅
  - **#326** - Centralized timekeeper abstraction
    - Dedicated timekeeper structure
    - Unified time management logic
    - Dependencies: #286 ✅, #325
  - **#327** - clock_nanosleep() and timerfd support
    - clock_nanosleep() syscall with TIMER_ABSTIME
    - timerfd for event-driven time waits
    - Dependencies: #287 ✅, #326, #96 ✅
  - **#328** - ITIMER_VIRTUAL and ITIMER_PROF support
    - CPU time tracking (user vs kernel)
    - ITIMER_VIRTUAL (SIGVTALRM)
    - ITIMER_PROF (SIGPROF)
    - Dependencies: #288 ✅, #213 ✅, #34 ✅
  - **#329** - Extended clock IDs (MONOTONIC_RAW, BOOTTIME, CPU clocks)
    - CLOCK_MONOTONIC_RAW, CLOCK_BOOTTIME
    - CLOCK_PROCESS_CPUTIME_ID, CLOCK_THREAD_CPUTIME_ID
    - Dependencies: #328, #326, #109
  - **#330** - Basic timezone support (TZ environment variable)
    - TZ parsing, localtime() awareness
    - mktime() and strftime() timezone support
    - Dependencies: #290 ✅, #307 ✅
  - Issue #226 (RTC and time management) partially complete: 7/13 issues done
  - Total project issues: 95 (was 89)
  - Overall progress: 87/95 issues complete (91.6%)
- **2025-10-20**: Created #331-#334 - **Test Infrastructure Fixes** 🧪
  - **#331** - Add framebuffer stubs for host test build ✅ CLOSED
    - 14 framebuffer function stubs
    - test/stubs.c now provides fb_* functions
  - **#332** - Add FAT32 adapter stubs for host test build ✅ CLOSED
    - 3 FAT32 directory operation stubs
    - fat32_mkdir/rmdir/rename_adapter stubs
  - **#333** - Add VM and keyboard stubs for host test build ✅ CLOSED
    - vm_map_physical, keyboard_event_*, proc_user_touch_range stubs
    - Syscall dependencies resolved
  - **#334** - Fix host test build linker errors (parent) ✅ CLOSED
    - make test now passes successfully
    - All host test linker errors resolved
  - Total project issues: 99 (was 95)
  - Overall progress: 91/99 issues complete (91.9%)
- **2025-10-20**: Created #335 - **PNP0A08 PCI Host Bridge Driver** 🚌
  - Centralized PCI enumeration and resource management
  - PCI driver registration API (Linux-style)
  - Eliminates duplication across PCI drivers (AHCI, e1000, USB, NVMe)
  - Foundational infrastructure for hardware support

- **2025-10-29**: Closed #191 (binutils port - 100% complete, all 11 tools working) ✅
- **2025-10-29**: Closed #365, #366, #367, #317 (complete filesystem metadata support) ✅
- **2025-10-29**: Closed #368 (pathconf - full POSIX support), #369 (system()), #370 (stat cmd) ✅
- **2025-10-29**: Closed #372 (shutdown cmd), #373 (realpath cmd), #374 (head cmd) ✅
- **2025-10-29**: Closed #376 (printf dynamic width), #387 (PCI boot listing with color coding) ✅
- **2025-10-29**: Closed #135 (Gcov integration), #236 (delete key), #350 (time cmd), #77 (tools docs) ✅
- **2025-10-29**: Created #382-#386 (audio subsystem - 5 comprehensive issues, 9-12 weeks planned)
- **2025-10-29**: Created #394 (terminal scrollback with mouse wheel - blocked by #143)
- **2025-10-29**: Updated #364 (stubbed functions tracker to 71% complete - 10/14 functions)
- **2025-10-29**: GCC milestone reaches 85.7% (6/7 complete) - only TCC port remaining!
- **2025-10-30**: 🎨 **GUI Milestone launched!** Created comprehensive GUI roadmap with 20 issues:
  - **Graphics Foundation**: #396 (Cairo), #397 (Pixman), #398 (FreeType)
  - **Input Stack**: #399 (input events), #400 (mouse cursor)
  - **IPC**: #401 (shared memory), #402 (Unix sockets)
  - **Compositor**: #403 (core), #404 (protocol), #405 (composition engine)
  - **Window Manager**: #406 (core), #407 (focus), #408 (decorations)
  - **Desktop**: #409 (shell), #410 (GUI apps)
  - **Infrastructure**: Assigned #143 (mouse), #129 (Unicode), #109 (pthread), #339 (thread-safe libc), #394 (scrollback) to GUI milestone
  - **Documentation**: Created [docs/road/road_to_gui.md](road/road_to_gui.md) - complete roadmap with architecture, code examples, 6 milestones (8-10 months)
  - **Other**: #411 (source file headers with MIT License)
  - Total project issues now: **127** (was 106), GUI milestone: 0/20 (just launched!)

---

**Last Updated**: 2025-10-30
**See Also**:
- [Road to Shell](road/road_to_shell.md)
- [Road to Buddy Allocator](road/road_to_buddy_allocator.md)
- [Road to GCC](road/road_to_gcc.md)
- [Road to Doom](road/road_to_doom.md)
- [Road to Real Hardware](road/road_to_real_hardware.md)
- [Road to GUI](road/road_to_gui.md) 🆕 - Complete graphical user interface stack
