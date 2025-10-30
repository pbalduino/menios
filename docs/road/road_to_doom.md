# Road to Doom on meniOS

🎯 **Goal**: Run the classic 1993 Doom game in userland on meniOS!

## 🎉 **MILESTONE COMPLETE - v0.1.666 "DOOM READY"**

**✅ ALL 33 REQUIRED ISSUES COMPLETE (100%)!** The Doom milestone has been fully achieved in release v0.1.666. All infrastructure needed to run the classic 1993 Doom game in userland is now implemented, tested, and integrated into the build system.

**Using [doomgeneric](https://github.com/ozkl/doomgeneric)**: meniOS integrates the excellent doomgeneric port by [@ozkl](https://github.com/ozkl), which provides a clean platform abstraction layer. The meniOS-specific implementation is in `vendor/genericdoom/doomgeneric_menios.c`, providing graphics, input, timing, and file I/O integration.

## 📊 **Progress Assessment**

**✅ Foundation Complete!** The core kernel infrastructure needed for userspace applications is now solidly implemented:

> For an up-to-date checklist of the interactive shell milestone (PID 1 supervision, console UX, and tool availability), see [`docs/road/road_to_shell.md`](./road_to_shell.md).

- ✅ **Memory Management**: Physical/virtual memory, kernel heap, mmap/munmap (Issues #35, #57, #89)
- ✅ **Process Scheduling**: Preemptive userland scheduler with time slicing (Issue #34)
- ✅ **Synchronization**: Mutexes and condition variables (Issues #36, #40)
- ✅ **User Mode Infrastructure**: Ring 3 transitions, syscall interface, ELF loader
- ✅ **Process Management**: File descriptors and fork/exec process creation (Issues #96, #93) — address spaces are duplicated eagerly (no COW yet).
- ✅ **Threading Foundation**: Kernel threading infrastructure complete (Issue #108)

**🔥 Ready for Next Phase**: With the foundation complete, we can now tackle application-level infrastructure!

## 🏗️ **Remaining Infrastructure for Doom**

Below is the roadmap of infrastructure we still need before a vanilla Doom port can enter userland, organized by priority and dependencies.

### **Phase 1: Process & I/O Management** ✅ **COMPLETE**
Core process and memory plumbing is in tree, and the minimal shell milestone builds on this layer.

#### **File Descriptor Management** (Issue #96) ✅ **COMPLETE**
- ✅ **Status**: dup/dup2, descriptor tables, and CLOEXEC support are implemented.
- **Impact**: Provides the descriptor backbone needed by files, pipes, and future sockets.

#### **Memory Mapping Syscalls** (Issue #89) ✅ **COMPLETE**
- ✅ **Status**: `mmap`/`munmap` back user-side allocators and file mappings.
- **Impact**: Enables dynamic heaps, loaders, and demand-style allocation.

#### **Fork/Exec Process Creation** (Issue #93) ✅ **COMPLETE (eager copy)**
- ✅ **Status**: `fork()`/`execve()` paths are implemented; address spaces are duplicated eagerly. Copy-on-write is still on the roadmap.
- **Impact**: Allows init, shells, and multi-process workloads to spawn child programs.

### **Phase 2: Threading Support** (HIGH PRIORITY - Foundation Complete!)
Complete multithreading infrastructure for modern applications:

#### **Kernel Threading Infrastructure** (Issue #108) ✅ **COMPLETE**
- ✅ **Status**: CLOSED - Thread Control Blocks, thread scheduling, stack management
- **Impact**: Multithreaded applications foundation - ENABLED

#### **pthread API Implementation** (Issue #109)
- ✅ **Status**: Ready to implement NOW (kernel threading complete #108 ✅)
- **Scope**: Full POSIX threading API (create/join/exit, attributes, TSD)
- **Impact**: Standard threading interface for applications

#### **Thread-Safe C Library** (Issue #110)
- **Dependencies**: Issue #109 (pthread API)
- **Scope**: Thread-safe malloc, stdio, errno, locale functions
- **Impact**: Enables safe multithreaded programming

### **Phase 3: Advanced IPC** (READY TO IMPLEMENT!)
Inter-process communication for complex applications:

#### **Pipes and FIFOs** (Issues #206-#209, broken down from #102) ✅ **COMPLETE!**
- ✅ **Status**: **FULLY COMPLETE!** #206 ✅ #207 ✅ #208 ✅ #209 ✅
- **Implementation Path**:
  1. ✅ **#206** - Pipe data structure and kernel control path (CLOSED)
  2. ✅ **#207** - Pipe syscall and userspace API (CLOSED)
  3. ✅ **#208** - Shell pipeline integration (CLOSED)
  4. ✅ **#209** - Named FIFOs (mkfifo) (CLOSED)
- **Impact**: Shell operations, process communication, command pipelines, named FIFOs - **ALL IMPLEMENTED!** 🎉

#### **UNIX Signals** (Issues #210-#213, broken down from #103) ✅ **ALL COMPLETE!**
- ✅ **Status**: All signal infrastructure complete! Full POSIX signal subsystem operational.
- **Implementation Path**:
  1. ✅ **#210** - Signal bookkeeping scaffold (CLOSED)
  2. ✅ **#211** - Signal syscalls (`kill`, `sigaction`, `sigprocmask`) + libc wrappers and regression tests (CLOSED)
  3. ✅ **#212** - Basic signal delivery path with user handlers (CLOSED)
  4. ✅ **#213** - Shell Ctrl+C integration (CLOSED)
  5. ✅ **#103** - UNIX signals parent issue (CLOSED)
- **Impact**: Process control, Ctrl+C handling, error handling, graceful shutdown - **ALL IMPLEMENTED!** 🎉

#### **Shared Memory** (Issues #215-#219, broken down from #104) ✅ **COMPLETE!**
- ✅ **Status**: All shared memory infrastructure implemented and tested!
- **Implementation Path**:
  1. ✅ **#215** - Kernel shared memory region manager (CLOSED)
  2. ✅ **#216** - Shared memory syscalls (shmget/shmat/shmdt) (CLOSED)
  3. ✅ **#217** - Reference counting and cleanup (CLOSED)
  4. ✅ **#218** - Comprehensive test suite (CLOSED)
  5. ✅ **#219** - Documentation and examples (CLOSED)
- **Impact**: Fast inter-process data sharing, audio/video buffers

#### **Device Control** (Issue #220)
- ✅ **Status**: ioctl dispatcher live; `/dev/tty0` now answers `TIOCGWINSZ`
- **#220** - ioctl syscall for device-specific operations
- **Impact**: Terminal control, framebuffer config, device management

### **Phase 4: File System & Storage** ✅ **MOSTLY COMPLETE**
We can mount and read from disk images today; write support is still limited.

#### **Block Device Driver** (Issue #62) ✅ **COMPLETE**
- ✅ **Status**: AHCI driver with DMA and interrupt completion ships in-tree.
- **Impact**: Provides the hardware path for loading data off the SATA image.

#### **Block Cache System** (Issue #63) ✅ **COMPLETE**
- ✅ **Status**: LRU block cache (`block_cache.c`) sits in front of block devices.
- **Impact**: Cuts down repeated DMA traffic and improves read latency.

#### **Filesystem Library** (Issue #64) ✅ **COMPLETE (read-only)**
- ✅ **Status**: FAT32 support handles GPT discovery, directory walks, and file reads.
- **Impact**: Kernel can traverse `/` and load assets from the disk image.

#### **VFS Layer** (Issue #65) ✅ **COMPLETE**
- ✅ **Status**: Generic mount table with path resolution (`vfs.c`) fronts filesystem drivers.
- **Impact**: Userland touches files by path regardless of the backing FS.

#### **File I/O Syscalls** (Issue #60) ✅ **COMPLETE (read-focused)**
- ✅ **Status**: `open`/`read`/`write`/`lseek`/`close` are wired through the VFS and descriptor tables.
- **Limitation**: FS writes remain largely TODO (FAT32 is currently read-only - see #189).
- **Impact**: Doom-sized assets can now be loaded from disk.

#### **Filesystem Write Support** (Issue #189) ✅ **COMPLETE**
- ✅ **Status**: FAT32 write support fully implemented! (#291, #292, #293 all done)
- **Scope**: File creation, modification, truncation with O_CREAT/O_TRUNC/O_EXCL
- **Impact**: Save games, configuration files, native compilation output - **ALL WORKING!**

#### **I/O Scheduler** (Issue #205) ✅ **COMPLETE**
- ✅ **Status**: Elevator-based block I/O scheduler is now implemented!
- **Impact**: Reduced seek latency for concurrent disk operations (shell + Doom asset streaming)
- **Benefits**: Better performance when multiple processes access disk simultaneously

#### **VFS Streaming I/O and Buffer Cache** (Issue #294) ✅ **COMPLETE**
- ✅ **Status**: VFS streaming I/O fully implemented! (#295, #296, #297, #298 all done)
- **Scope**: Block cache with LRU eviction, streaming read/write, read-ahead, write-behind
- **Impact**: Handles arbitrarily large files, efficient I/O for game asset loading, foundation for ext2

### **Phase 5: Graphics & Input**
Visual output and user interaction:

#### **Userspace Graphics Interface** (Issue #31) ✅ **COMPLETE**
- ✅ **Status**: `/dev/fb0` exposes the framebuffer to userland; console writes multiplex to serial+video.
- **Impact**: Games can blit directly to the screen today.

#### **Input Subsystem** (Issue #32) ✅ **COMPLETE**
- ✅ **Status**: PS/2 key events flow into a userspace-readable stdin ring buffer.
- **Impact**: Shells and future games can read input without polling hardware.

#### **Audio Subsystem** (Issue #33) 🚧 **Pending**
- **Scope**: PCM output, mixer/stream syscalls, timer-driven audio.
- **Requirements**: 8-bit/16-bit audio buffers for Doom sound.
- **Impact**: Game audio and sound effects (currently missing).

### **Phase 6: Toolchain and Build Flow**
Development environment for building applications:

#### **Cross-Compiler Toolchain** (Issue #29)
- **Scope**: binutils + GCC/Clang targeting meniOS userland ABI
- **Impact**: Compiling applications for meniOS

#### **C Runtime and libc**
- **Dependencies**: Threading support (Issues #109, #110)
- **Scope**: crt0 (#192), libc subset (#193), dynamic vs static linking decisions
- **Impact**: Standard library support for applications

#### **Syscall ABI Documentation** (Issue #194)
- **Scope**: Formal syscall interface documentation
- **Impact**: Developer reference for userland programming

#### **Userspace Build System** (Issue #195)
- **Dependencies**: Issues #192, #193
- **Scope**: Dedicated build system for userland programs
- **Impact**: Clean separation between kernel and userland builds

#### **Userspace SDK**
- **Dependencies**: Cross-compiler toolchain (#29)
- **Scope**: Headers, linker scripts, build system integration
- **Impact**: Reproducible builds for Doom and other applications

### **Phase 7: Userland Utilities**
Basic command-line tools for shell interaction:

#### **/bin Utilities** (Issue #183)
- **Scope**: echo, cat, env, true, false
- **Impact**: Basic shell operations and testing

#### **Process Management Tools** (Issue #187) ✅
- **Scope**: ps (list processes), kill (send signals)
- **Dependencies**: Issue #103 (signals), Issue #153 (procfs helpful but not required)
- **Impact**: Process monitoring and control from shell

### **Phase 8: libc Gaps for Doom** (NEW - 6 issues)
Missing C library functions that Doom requires for linking:

#### **File stdio Support** (Issue #305)
- **Status**: ✅ COMPLETE - buffered FILE implementation in libc
- **Dependencies**: #96 ✅, #189 ✅, #294 ✅, #193 ✅
- **Scope**:
  - Implement FILE structure with buffer, fd, position tracking
  - fopen/fclose for opening files with mode strings
  - fread/fwrite for buffered I/O operations
  - fseek/ftell/rewind for file positioning
  - fflush for explicit buffer writes
  - ferror/clearerr stubs for error handling
- **Impact**: Doom loads WAD files via fopen in w_file_stdc.c, config parsing in m_misc.c, g_game.c
-  **Priority**: CRITICAL - linker will fail without these symbols

#### **Filesystem Helpers** (Issue #306) ✅ **COMPLETE**
- ✅ **Status**: Complete - mkdir/remove/rename/unlink now in libc
- **Dependencies**: #193 ✅, #60 ✅, #65 ✅
- **Implemented**:
  - mkdir() for directory creation (M_MakeDirectory in Doom)
  - remove() for file deletion
  - rename() for file moves
  - unlink()/rmdir() for file deletion paths
- **Impact**: Doom save game management and config file handling now working
- **Priority**: HIGH - save games now functional

#### **Environment Variable Access** (Issue #307) ✅ **COMPLETE**
- ✅ **Status**: Complete - getenv/putenv now in libc
- **Dependencies**: #148 ✅, #193 ✅
- **Implemented**:
  - getenv() for reading environment variables
  - putenv() for adding/modifying variables
  - setenv/unsetenv for environment manipulation
  - environ pointer properly maintained
- **Impact**: Doom DOOMWADDIR and SDL_VIDEODRIVER environment variable support now working
- **Priority**: HIGH - IWAD detection now functional

#### **String Utilities** (Issue #308) ✅ **COMPLETE**
- ✅ **Status**: `strdup`, `strndup`, `strcasecmp`, and `strncasecmp` now ship in libc.
- **Dependencies**: #95 ✅, #193 ✅
- **Highlights**:
  - ✅ Implemented POSIX-compatible `strdup`/`strndup` with proper `errno` handling.
  - ✅ Added case-insensitive compare helpers (`strcasecmp`, `strncasecmp`) using `ctype.h`.
  - ✅ Updated `<string.h>` prototypes so Doom—and future ports—build cleanly.
- **Impact**: Doom’s IWAD/config parsing now links without stubbed utilities.
- **Next**: Proceed with the remaining libc gaps (#305-#307/#309/#310).

#### **Formatted I/O Exposure** (Issue #309) ✅ **COMPLETE**
- ✅ **Status**: Complete - snprintf/vsnprintf/vsprintf now exported
- **Dependencies**: #193 ✅, #304 ✅
- **Implemented**:
  - snprintf() for safe formatted strings
  - vsnprintf() for variadic version
  - vsprintf() for unsafe version (compatibility)
  - Exposed in <stdio.h> with proper exports
- **Impact**: Doom DeHackEd patches (DEH_snprintf) and config generation (M_vsnprintf) now working
- **Priority**: CRITICAL - required symbols now available

#### **Math Library** (Issue #310)
- **Status**: ✅ COMPLETE - fabs/fabsf/fabsl implemented and math.h refreshed
- **Dependencies**: #193 ✅
- **Scope**:
  - fabs() for floating-point absolute value
  - fabsf() for float version (optional)
  - Clean up <math.h> header
  - Use compiler builtin if available
- **Impact**: Doom video scaling calculations in v_video.c
- **Priority**: HIGH - only function Doom needs from libm

### **Phase 9: Doom Integration** (4 existing + 2 new = 6 issues)
Port layer, graphics, input, and build integration for running Doom:

#### **Pixel-Addressable Framebuffer** (Issue #301)
- **Status**: TODO - `/dev/fb0` currently only supports text mode
- **Dependencies**: #31 ✅, #89 ✅, #220 ✅
- **Scope**:
  - mmap support for direct video RAM access
  - ioctl interface for geometry/format queries (FBIOGET_VSCREENINFO)
  - Expose width, height, pitch, pixel format to userland
- **Current Gap**: `src/kernel/file.c:507` routes through `fb_putchar` only
- **Impact**: DG_DrawFrame() needs raw scanline blitting to video memory

#### **Real Key Events Delivery** (Issue #302) ✅ **COMPLETE**
- ✅ **Status**: Complete - PS/2 driver now reports both press and release events
- **Dependencies**: #32 ✅, #220 ✅
- **Implemented**:
  - Both key press AND release events with scan codes
  - Scan codes preserved and accessible to userland
  - Modifier key state tracking (Shift, Ctrl, Alt)
  - Extended scan codes (0xE0 prefix) handled correctly
  - Event-based interface for keyboard input
- **Impact**: Doom input loop can now properly handle key up/down events for movement and controls

#### **scanf Family Implementation** (Issue #304) ✅ **COMPLETE**
- ✅ **Status**: Complete scanf family implementation with C99 format support
- **Dependencies**: #193 ✅
- **Implemented**:
  - sscanf(), scanf(), fscanf() and variadic counterparts (vsscanf, vscanf, vfscanf)
  - Core format parser for %d, %i, %u, %x, %o, %f, %s, %c, %n, %[...] specifiers
  - Width specifiers and assignment suppression (*)
  - Length modifiers (hh, h, l, ll, L, z, t)
- **Impact**: Doom config parsing (vendor/genericdoom/m_config.c, m_misc.c) now functional

#### **Wire Up meniOS Port Layer** (Issue #300) ✅ **COMPLETE**
- ✅ **Status**: Doomgeneric now drives meniOS graphics, input, and timing end-to-end.
- **Dependencies**: #301 ✅, #302 ✅, #287 ✅, #240 ✅, #304 ✅
- **Highlights**:
  - ✅ **DG_Init()** acquires `/dev/fb0`, adjusts video mode when needed, mmaps the framebuffer, and registers orderly shutdown.
  - ✅ **DG_DrawFrame()** blits frames directly into mapped video memory and flushes via `MENIOS_FB_IOCTL_FLUSH`.
  - ✅ **DG_GetKey()/DG_SleepMs()/DG_GetTicksMs()** hook into `menios_input_poll()`, `nanosleep()`, and `clock_gettime()` for responsive gameplay timing.
- **Impact**: Port layer is production-ready; remaining Doom work focuses on libc gaps (#305-#310) and build integration steps (#311-#312).

#### **Doom meniOS-Specific Build System** (Issue #311) ✅ **COMPLETE**
- ✅ **Status**: meniOS now ships a dedicated Doom build flow.
- **Highlights**:
  - ✅ Added `vendor/genericdoom/Makefile.menios` that compiles and links `build/bin/doom.elf` with the meniOS SDK.
  - ✅ Top-level `make doom` target builds the full binary (Docker-aware) for rapid iteration.
  - ✅ Extended SDK headers (`stdio.h`, `stdlib.h`, `string.h`, etc.) so Doom sources compile cleanly.
- **Impact**: Build infrastructure is done; final packaging and install steps move to #312 after libc work.

#### **Doom Build Integration** (Issue #312)
- **Status**: TODO - Doom not integrated into main build
- **Dependencies**: #311 ✅ (Doom Makefile), #192 ✅, #193 ✅, #195 ✅, #29 ✅
- **Scope**:
  - Add Doom to `make userland` target
  - Place binary at `$(OUTPUT_DIR)/bin/doom`
  - Copy doom.wad into disk image if present
  - Guard WAD copy so builds succeed without it
  - Update top-level Makefile
- **Current Gap**: Doom not part of automated build
- **Impact**: Manual build steps required, not integrated with image creation

#### **Old Build Integration** (Issue #303) - Superseded
- **Status**: Superseded by #311 ✅ and #312
- **Note**: Original build integration issue, now split into build system (#311) and integration (#312)

## 🎮 **Doom-Specific Requirements**

### **Memory Requirements**
- **Heap Space**: ~8-16 MiB for game data and assets
- **Stack Space**: Standard per-thread stacks (implemented ✅)
- **Asset Loading**: WAD file support via filesystem (Phase 4)

### **Graphics Requirements**
- **Resolution**: 320×200 paletted mode (classic) or 640×480 higher color
- **Double Buffering**: Page-flip syscall to avoid tearing
- **Palette Control**: VGA palette manipulation for classic graphics

### **Audio Requirements**
- **PCM Output**: 8-bit/16-bit audio buffer support
- **Sample Rate**: 11-44 kHz support for sound effects and music
- **Mixing**: Software mixing for multiple audio streams

### **Input Requirements**
- **Keyboard**: Arrow keys, WASD, space, enter, escape
- **Mouse**: Relative motion and button presses for looking/turning
- **Optional**: Gamepad support via HID interface

### **File System Requirements**
- **WAD Loading**: Read game assets from filesystem ✅
- **Save Games**: Write/read save game files (#189 - FAT32 write support)
- **Configuration**: Config file support for game settings (#189)

## 📈 **Updated Timeline Estimates**

### **Short Term (3-6 months)**
- Land pthread API and libc hardening (#109, #110, #113).
- ✅ Add core IPC plumbing: pipes ✅ (#102), signals ✅ (#103 - ALL COMPLETE!), shared memory ✅ (#104).
- Keep roadmap docs in sync with kernel progress.

### **Medium Term (6-12 months)**
- Introduce audio output (#33) and network/socket layers (#67-#71).
- Stand up the cross-compiler toolchain and SDK (#29).

### **Long Term (12+ months)**
- Optimise for SMP workloads and per-CPU data (#80-#86).
- Deliver polished user tooling, networking extras (#72-#73), and the Doom port itself.

## 🚀 **Immediate Next Steps**

**High-impact kernel work still outstanding:**

### Toolchain (Critical Path)
1. **#192** – Implement crt0 runtime startup code ✅ *Completed*
2. **#193** – Build minimal userland libc (syscalls, strings, memory, stdio) ✅ *Completed*
3. **#194** – Document syscall ABI specification
4. **#195** – Separate userland build system from kernel
5. **#29** – Complete cross-compiler toolchain integration

### Threading & IPC
6. ✅ **#108** – Kernel threading infrastructure ✅ *Completed*
7. **#109** – Implement the pthread API so user programs can spin up threads (ready now!)
8. **#110** – Make libc thread-safe once pthread primitives exist

**IPC - Pipes** (sequential): ✅ **COMPLETE!**
10. ✅ **#206** – Pipe data structure and kernel control path ✅ *Completed*
11. ✅ **#207** – Pipe syscall and userspace API ✅ *Completed*
12. ✅ **#208** – Shell pipeline integration ✅ *Completed*
13. ✅ **#209** – Named FIFOs (mkfifo) ✅ *Completed*

**IPC - Signals** (sequential): ✅ **COMPLETE!**
14. ✅ **#210** – Signal bookkeeping scaffold ✅ *Completed*
15. ✅ **#211** – Signal syscalls (kill, sigaction, sigprocmask, sigsuspend) ✅ *Completed*
16. ✅ **#212** – Basic signal delivery path with user handlers ✅ *Completed*
17. ✅ **#213** – Shell Ctrl+C integration ✅ *Completed*
18. ✅ **#103** – UNIX signals parent issue ✅ *Completed*

**IPC - Shared Memory** (sequential): ✅ **COMPLETE!**
19. ✅ **#215** – Kernel shared memory region manager ✅ *Completed*
20. ✅ **#216** – Shared memory syscalls (shmget, shmat, shmdt, shmctl) ✅ *Completed*
21. ✅ **#217** – Reference counting and cleanup ✅ *Completed*
22. ✅ **#218** – Comprehensive test suite ✅ *Completed*
23. ✅ **#219** – Documentation and examples ✅ *Completed* *(docs/design/shared_memory.md)*

**IPC - Other**:
24. ✅ **#220** – ioctl syscall for device-specific operations ✅ *Completed*
25. ✅ **#221** – Fast syscall/sysret path ✅ *Completed* - proper 64-bit return values, all syscalls use SYSCALL instruction

### System Services
26. **#33** – Bring up the audio subsystem for Doom's sound effects/music
27. ✅ **#189** – Add FAT32 write support for save games and config files ✅ *Completed*
28. ✅ **#205** – Elevator I/O scheduler ✅ *Completed*
29. ✅ **#183** – Provide basic `/bin` utilities (echo, cat, env, true, false) ✅ *Completed*
30. ✅ **#185** – PATH search configuration ✅ *Completed*
31. ✅ **#187** – Add process management tools (ps, kill) ✅ *Completed*
32. ✅ **#188** – env utility ✅ *Completed*
33. ✅ **#286** – Kernel RTC driver and time management ✅ *Completed*
34. ✅ **#287** – nanosleep() and sleep queue ✅ *Completed*
35. ✅ **#288** – Interval timers (setitimer) ✅ *Completed*
36. ✅ **#290** – Time conversion utilities (gmtime/mktime/strftime) ✅ *Completed*

### Future: Native Compilation
37. **#190** – Port TCC (Tiny C Compiler) to meniOS
38. **#191** – Port binutils (as, ld) for native development

These items unlock the bulk of the remaining roadmap phases (threaded libc, IPC,
networking) and pave the way for shipping a Doom-capable user environment.

## 🎯 **Success Criteria**

meniOS will be ready for Doom when we can:
- ✅ Boot into userspace (COMPLETE)
- ✅ Run ELF executables (COMPLETE)
- ✅ Basic syscall interface (COMPLETE)
- [ ] Load and execute Doom binary
- [ ] Access WAD files from filesystem
- [ ] Display graphics to framebuffer
- [ ] Capture keyboard and mouse input
- [ ] Play audio through sound system
- [ ] Save and load game state

## 🏆 **The Vision**

When complete, users will be able to:
1. Boot meniOS
2. Launch a shell or menu program
3. Run `doom -file doom1.wad`
4. Play the full Doom experience with:
   - Smooth graphics rendering
   - Responsive controls
   - High-quality audio
   - Save/load functionality
   - Stable performance

**Current Status**: 🟢 **Foundation Complete** - Ready for rapid feature development!

The solid foundation work (memory management, scheduling, synchronization) now enables parallel development across multiple tracks, significantly accelerating the path to running Doom in userland.

---

## 🎯 **GitHub Milestone Tracking**

The Doom milestone on GitHub now tracks 33 issues:
- **Status**: 33/33 complete (100%) 🎉
- **Completed**:
  - Graphics & Input: #31 ✅, #32 ✅, #136 ✅ (devfs), #140 ✅ (/dev/kbd0 and /dev/fb0)
  - Memory: #95 ✅
  - File System: #189 ✅ (FAT32 writes complete!)
  - IPC - Pipes: #102 ✅ (parent)
  - IPC - Signals: #103 ✅ (parent), #210 ✅, #211 ✅, #212 ✅, #213 ✅ - **ALL 5 COMPLETE!** 🎉
  - IPC - Shared Memory: #215 ✅, #216 ✅, #217 ✅, #218 ✅, #219 ✅ (ALL COMPLETE!)
  - IPC - Other: #220 ✅ (ioctl), #221 ✅ (fast syscalls)
  - libc Gaps: #305 ✅ (file stdio), #306 ✅ (filesystem helpers), #307 ✅ (environment access), #308 ✅ (string utilities), #309 ✅ (formatted I/O), #310 ✅ (math library) - **ALL 6 COMPLETE!** 🎉
  - Doom Integration: #300 ✅ (meniOS port layer), #301 ✅ (framebuffer mmap), #302 ✅ (real key events), #303 ✅ (build integration - superseded), #304 ✅ (scanf family), #311 ✅ (Doom build system), #312 ✅ (full build integration)
  - Stability: #262 ✅ (user-mode page fault handling), #274 ✅ (init crash fix), #319 ✅ (Doom null pointer crash), #320 ✅ (shell hang after Doom crash), #322 ✅ (shell alarm crash), #323 ✅ (shell crash after Doom exit)
- **New Issues Created (2025-10-18/19)**:
  - libc Gaps: #305 (file stdio), #306 (filesystem helpers), #307 (environment access), #309 (formatted I/O), #310 (math library)
  - Build System: #312 (build integration)
  - Doom Integration: #303 (old build - superseded)
- **🎉 MILESTONE COMPLETE!** All required issues closed!
- **Removed from milestone**: #33 (Audio), #109 (pthread API), #110 (thread-safe libc), #111 (advanced pthread), #112 (thread debugging), #113 (threading utilities), #143 (mouse driver), #105 (Unix sockets), #106 (microkernel IPC), #107 (capabilities), #214 (Advanced signal features) - moved to future work / not required for initial Doom port

**Recent Major Achievements**:
- ✅ FAT32 write support (#189, #291-#293) - Save games now possible!
- ✅ VFS streaming I/O (#294-#298) - Efficient asset loading ready!
- ✅ Time management (#286, #287, #288, #290) - Complete timing system for game loop!
- ✅ Fast syscalls (#221) - High-performance system calls with 64-bit returns!
- ✅ scanf family (#304) - Config file parsing fully implemented!
- ✅ Real key events (#302) - Keyboard input with scan codes and up/down events!
- ✅ Device filesystem (#136, #140) - /dev/kbd0 and /dev/fb0 production-ready!
- ✅ Pixel-addressable framebuffer (#301) - `/dev/fb0` mmap and flush path ready for userland!
- ✅ Doom port layer (#300) - COMPLETE: graphics, input, and timing all wired through meniOS!
- ✅ Doom build system (#311) - COMPLETE: meniOS Makefile + tooling can rebuild the entire Doom codebase!
- ✅ String utilities (#308) - strdup, strcasecmp, strncasecmp now in libc!
- ✅ File stdio (#305) - fopen/fclose/fread/fwrite now in libc for WAD loading!
- ✅ Filesystem helpers (#306) - mkdir/remove/rename now in libc for save games!
- ✅ Environment access (#307) - getenv/putenv now in libc for Doom config!
- ✅ Formatted I/O (#309) - snprintf/vsnprintf/vsprintf properly exposed!
- ✅ Math library (#310) - fabs/fabsf/fabsl now in libc for video scaling!
- 🎉 **ALL 6 LIBC GAPS COMPLETE!** Doom build integration (#312) now fully unblocked!
- ✅ Shell crash after Doom exit (#323) - Fixed syscall frame restoration on process resume!
- ✅ Shell hang after Doom crash (#320) - Fixed waitpid() handling for abnormal child termination!
- ✅ Shell alarm crash (#322) - Fixed signal frame setup and stack management!
- ✅ Doom build integration (#303, #312) - Doom now ships automatically with every meniOS build!

See [MILESTONES.md](../MILESTONES.md) for detailed milestone tracking across all three major goals (Mosh, GCC, Doom).

---

**Last Updated**: 2025-10-20
**Status**: 🎉 **MILESTONE COMPLETE - Released in v0.1.666 "DOOM READY"**
**Achievement**: All 33 required issues complete. Doom can now run in userland with full graphics, input, file I/O, and process management support.
**Next Steps**: Native compilation (TCC #190, binutils #191), threading support (#109, #110), audio subsystem (#33)
