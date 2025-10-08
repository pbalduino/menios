# Road to Doom on meniOS

🎯 **Goal**: Run the classic 1993 Doom game in userland on meniOS!

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
- ✅ **Status**: COMPLETED - Thread Control Blocks, thread scheduling, stack management
- **Impact**: Multithreaded applications foundation - ENABLED

#### **pthread API Implementation** (Issue #109)
- ✅ **Status**: Ready to implement (kernel threading complete)
- **Scope**: Full POSIX threading API (create/join/exit, attributes, TSD)
- **Impact**: Standard threading interface for applications

#### **Thread-Safe C Library** (Issue #110)
- **Dependencies**: Issue #109 (pthread API)
- **Scope**: Thread-safe malloc, stdio, errno, locale functions
- **Impact**: Enables safe multithreaded programming

#### **Advanced pthread Synchronization** (Issue #111)
- **Dependencies**: Issue #109 (pthread API)
- **Scope**: Barriers, spinlocks, reader-writer locks, robust mutexes
- **Impact**: High-performance synchronization for complex applications

### **Phase 3: Advanced IPC** (READY TO IMPLEMENT!)
Inter-process communication for complex applications:

#### **Pipes and FIFOs** (Issue #102)
- ✅ **Status**: Ready to implement (file descriptors complete, condition variables complete)
- **Scope**: pipe(), mkfifo(), bidirectional communication
- **Impact**: Shell operations, process communication

#### **UNIX Signals** (Issue #103)
- ✅ **Status**: Ready to implement (fork/exec complete, waiting on Issue #101 timers)
- **Scope**: Signal delivery, handlers, masks, default actions
- **Impact**: Process control, error handling, graceful shutdown

#### **Shared Memory** (Issue #104)
- ✅ **Status**: Ready to implement (VM manager complete)
- **Scope**: shmget/shmat/shmdt for high-performance IPC
- **Impact**: Fast inter-process data sharing

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

#### **Filesystem Write Support** (Issue #189) 🚧 **Pending**
- **Scope**: FAT32 write support for file creation, modification, deletion
- **Impact**: Save games, configuration files, native compilation output

#### **I/O Scheduler** (Issue #205) 🚧 **Pending**
- **Scope**: Elevator-based block I/O scheduler for improved disk performance
- **Dependencies**: Issues #114, #62, #63 (all complete)
- **Impact**: Reduced seek latency for concurrent disk operations (shell + Doom asset streaming)

### **Phase 5: Graphics & Input**
Visual output and user interaction:

#### **Userspace Graphics Interface** (Issue #31) ✅ **COMPLETE**
- ✅ **Status**: `/dev/fb/0` exposes the framebuffer to userland; console writes multiplex to serial+video.
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

#### **Process Management Tools** (Issue #187)
- **Scope**: ps (list processes), kill (send signals)
- **Dependencies**: Issue #103 (signals), Issue #153 (procfs helpful but not required)
- **Impact**: Process monitoring and control from shell

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
- Land pthread API and libc hardening (#109-#111, #110).
- Add core IPC plumbing: pipes, signals, shared memory (#102-#104).
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
1. **#192** – Implement crt0 runtime startup code
2. **#193** – Build minimal userland libc (syscalls, strings, memory, stdio) ✅ *Completed*
3. **#194** – Document syscall ABI specification
4. **#195** – Separate userland build system from kernel
5. **#29** – Complete cross-compiler toolchain integration

### Threading & IPC
6. **#109** – Implement the pthread API so user programs can spin up threads
7. **#110** – Make libc thread-safe once pthread primitives exist
8. **#111** – Land advanced pthread synchronization (barriers, robust locks)
9. **#102** – Add pipes/FIFOs for shell pipelines and IPC
10. **#103** – Deliver UNIX signals so processes can be controlled from the shell
11. **#104** – Wire shared memory to back high-performance IPC (and future audio)

### System Services
12. **#33** – Bring up the audio subsystem for Doom's sound effects/music
13. **#189** – Add FAT32 write support for save games and config files
14. **#205** – Implement elevator I/O scheduler for better disk performance
15. **#183** – Provide basic `/bin` utilities (echo, cat, env, true, false)
16. **#187** – Add process management tools (ps, kill)

### Future: Native Compilation
17. **#190** – Port TCC (Tiny C Compiler) to meniOS
18. **#191** – Port binutils (as, ld) for native development

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
