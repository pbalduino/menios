# Road to Doom on meniOS

🎯 **Goal**: Run the classic 1993 Doom game in userland on meniOS!

## 📊 **Progress Assessment**

**✅ Core kernel and storage stack are operational.**
- Physical and virtual memory management with copy-on-write fork/exec and `mmap`/`munmap`
- Preemptive scheduler, kernel threads, sleep/yield, and full Ring 3 syscall path
- File descriptors, CLOEXEC, anonymous pipes, and libc syscall shims (`read`, `write`, `open`, `close`, `lseek`, `dup`, `fcntl`)
- PCI/AHCI DMA driver, block cache, GPT scanning, FAT32 filesystem driver, and VFS mount at `/`
- Embedded user demo exercises pipes, fork, filesystem reads, and priority scheduling with serial diagnostics

**🟡 Focus now shifts to userland runtime and device interfaces.**
- pthread API, thread-safe libc, and advanced synchronization for multithreaded apps
- Signals, timers, shared memory, futex/message IPC, and process coordination
- Writable filesystem path, framebuffer protocol, input/audio device interfaces, and Doom-specific services
- Cross-compilation toolchain and SDK needed to build and ship userspace binaries

## 🏗️ **Remaining Infrastructure for Doom**

Below is the roadmap of infrastructure we still need before a vanilla Doom port can enter userland, organized by priority and dependencies.

### **Phase 1: Process & I/O Management** ✅ **COMPLETE!**
Foundation process management infrastructure is now implemented:

#### **File Descriptor Management** (Issue #96) ✅ **COMPLETE**
- ✅ **Status**: COMPLETED - dup/dup2 operations, FD table management, close-on-exec support
- **Impact**: Foundation for all I/O operations (files, pipes, sockets) - ENABLED

#### **Memory Mapping Syscalls** (Issue #89) ✅ **COMPLETE**
- ✅ **Status**: COMPLETED - mmap/munmap for userspace memory allocation and file mapping
- **Impact**: Userspace heap allocators and large memory allocations - ENABLED

#### **Fork/Exec Process Creation** (Issue #93) ✅ **COMPLETE**
- ✅ **Status**: COMPLETED - Complete process lifecycle with copy-on-write memory
- **Impact**: Running separate programs and shell operations - ENABLED

### **Phase 2: Threading Support** (HIGH PRIORITY - Foundation Complete!)
Complete multithreading infrastructure for modern applications:

#### **Kernel Threading Infrastructure** (Issue #108) ✅ **COMPLETE**
- ✅ **Status**: COMPLETED - Thread Control Blocks, thread scheduling, stack management
- **Impact**: Multithreaded applications foundation - ENABLED

#### **pthread API Implementation** (Issue #109)
- 🟡 **Status**: Ready to implement (kernel threading complete)
- **Scope**: Full POSIX threading API (create/join/exit, attributes, TSD)
- **Impact**: Standard threading interface for applications

#### **Thread-Safe C Library** (Issue #110)
- ⏳ **Status**: Pending pthread API (#109)
- **Scope**: Thread-safe malloc, stdio, errno, locale functions
- **Impact**: Enables safe multithreaded programming

#### **Advanced pthread Synchronization** (Issue #111)
- ⏳ **Status**: Pending pthread API (#109)
- **Scope**: Barriers, spinlocks, reader-writer locks, robust mutexes
- **Impact**: High-performance synchronization for complex applications

### **Phase 3: Advanced IPC** (In Progress)
Inter-process communication for complex applications:

#### **Pipes and FIFOs** (Issue #102)
- ✅ **Status**: COMPLETE – anonymous pipes live in `src/kernel/fs/pipe.c`; `sys_pipe` installs read/write descriptors with blocking semantics
- **Impact**: Shell pipelines, parent/child hand-off, and Doom's streaming needs

#### **UNIX Signals** (Issue #103)
- 🟡 **Status**: Ready to implement once timer services (#101) land
- **Scope**: Signal delivery, handlers, masks, default actions
- **Impact**: Process control, crash handling, cooperative shutdown

#### **Shared Memory** (Issue #104)
- 🟡 **Status**: Ready to implement (VM manager complete, `mmap` groundwork done)
- **Scope**: `shmget`/`shmat`/`shmdt` APIs plus VFS hooks for POSIX shared memory
- **Impact**: Fast inter-process data sharing for render/audio pipelines

#### **Futex & Message IPC** (Issues #105-#107)
- 🔜 **Status**: Planned follow-ups after shared memory
- **Scope**: Futex-style wakeups, message queues, and cross-process synchronization
- **Impact**: Efficient event loops, sound mixer coordination, microkernel services

### **Phase 4: File System & Storage** (Complete, write support pending)
Persistent storage for game assets and save files:

#### **Block Device Driver** (Issue #62)
- ✅ **Status**: COMPLETE – PCI/AHCI DMA driver with interrupts and port discovery
- **Impact**: Direct disk access for boot media and asset streaming

#### **Block Cache System** (Issue #63)
- ✅ **Status**: COMPLETE – Global LRU cache to avoid redundant SATA transfers
- **Impact**: Faster filesystem traversal and reduced DMA pressure

#### **Filesystem Library** (Issue #64)
- ✅ **Status**: COMPLETE – GPT-aware FAT32 parser with long filename support
- **Impact**: Structured file storage and retrieval from install media

#### **VFS Layer** (Issue #65)
- ✅ **Status**: COMPLETE – Path normalization, mount table, and per-file buffering
- **Impact**: Uniform namespace for block devices, pipes, framebuffer, and more

#### **File I/O Syscalls** (Issue #60)
- ✅ **Status**: COMPLETE – `open`/`read`/`write`/`lseek`/`close` available to userland
- **Impact**: Loading WAD files, configuration, and runtime assets

#### **Filesystem Write Support** (Issue #61)
- 🟡 **Status**: Planned – extend FAT32/VFS to support file creation, write-back, and save-game persistence
- **Impact**: Doom save files, config serialization, mod support

### **Phase 5: Graphics & Input**
Visual output and user interaction:

#### **Userspace Graphics Interface** (Issue #31)
- 🟢 **Status**: COMPLETE – framebuffer info/map/flip syscalls expose a double-buffered staging surface for userland rendering; follow-up work will add mode switching & palette control as separate tasks
- **Scope**: Framebuffer mapping, double buffering, palette control
- **Impact**: Doom rendering pipeline and general GUI support

#### **Input Subsystem** (Issue #32)
- ✅ **Status**: KEYBOARD COMPLETED – PS/2 keyboard events exposed via `/dev/input/kbd`
- 🟡 **Status**: MOUSE PENDING – PS/2 mouse (#143) and USB mouse (#144) support planned
- **Scope**: Userspace keyboard/mouse interface, focus management
- **Impact**: Game controls, shell interaction, debugging tools

#### **Audio Subsystem** (Issue #33)
- 🟡 **Status**: Scoped – dependent on timers and streaming syscalls
- **Scope**: PCM output, mixer/stream syscalls, timer-driven audio
- **Impact**: Doom sound effects and music playback

### **Phase 6: Toolchain and Build Flow**
Development environment for building applications:

#### **Cross-Compiler Toolchain** (Issue #29)
- 🟡 **Status**: Design under discussion; requires libc ABI decisions
- **Scope**: binutils + GCC/Clang targeting meniOS userland ABI
- **Impact**: Compiling applications for meniOS

#### **C Runtime and libc**
- 🟡 **Status**: Blocked on pthread API (#109) and thread-safe libc work (#110)
- **Scope**: `crt0`, libc subset, dynamic vs static linking decisions
- **Impact**: Standard library support for applications

#### **Userspace SDK**
- 🔜 **Status**: Planned once toolchain solidifies
- **Scope**: Headers, linker scripts, build system integration
- **Impact**: Reproducible builds for Doom and other applications

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
- **WAD Loading**: Read game assets from filesystem
- **Save Games**: Write/read save game files
- **Configuration**: Config file support for game settings

## 📈 **Updated Timeline Estimates**

### **Short Term (0-3 months)**
- Ship pthread API and thread-safe libc foundations (#109, #110)
- Add thread-aware syscalls, profiling hooks, and scheduler tooling (#112, #113)
- Implement timer services and UNIX signals (#101, #103)

### **Medium Term (3-9 months)**
- Deliver shared memory and futex/message IPC primitives (#104-#107)
- Enable filesystem write support and VFS updates for save games (#61)
- Expose framebuffer, input, and audio interfaces to userland (#31-#33)

### **Long Term (9+ months)**
- Package cross-compiler toolchain and userspace SDK (#29)
- Stand up networking stack and sockets (#67-#73)
- Integrate Doom assets, rendering, audio, and save-game flows on meniOS

## 🚀 **Immediate Next Steps**

**Ready to implement now** (dependencies cleared):
1. **#109** – pthread API skeleton and thread lifecycle helpers
2. **#110** – Thread-safe libc (malloc/stdio/errno hardening)
3. **#113** – Thread-aware syscalls and scheduler inspection hooks

**High impact for applications**:
4. **#103/#101** – UNIX signals built atop the timer service
5. **#104** – Shared memory primitives for high-bandwidth IPC
6. **#31** – Userspace framebuffer interface to unblock rendering

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
