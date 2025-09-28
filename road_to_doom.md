# Road to Doom on meniOS

🎯 **Goal**: Run the classic 1993 Doom game in userland on meniOS!

## 📊 **Progress Assessment**

**✅ Foundation Complete!** The core kernel infrastructure needed for userspace applications is now solidly implemented:

- ✅ **Memory Management**: Physical/virtual memory, kernel heap, mmap/munmap (Issues #35, #57, #89)
- ✅ **Process Scheduling**: Preemptive userland scheduler with time slicing (Issue #34)
- ✅ **Synchronization**: Mutexes and condition variables (Issues #36, #40)
- ✅ **User Mode Infrastructure**: Ring 3 transitions, syscall interface, ELF loader
- ✅ **Process Management**: File descriptors, fork/exec process creation (Issues #96, #93)
- ✅ **Threading Foundation**: Kernel threading infrastructure complete (Issue #108)

**🔥 Ready for Next Phase**: With the foundation complete, we can now tackle application-level infrastructure!

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

### **Phase 4: File System & Storage**
Persistent storage for game assets and save files:

#### **Block Device Driver** (Issue #62)
- **Scope**: AHCI/ATA or RAM-backed disk with DMA support
- **Impact**: Hardware interface for storage devices

#### **Block Cache System** (Issue #63)
- **Dependencies**: Issue #62 (block device)
- **Scope**: Buffer management, write-back cache, performance optimization
- **Impact**: Efficient disk I/O operations

#### **Filesystem Library** (Issue #64)
- **Dependencies**: Issue #63 (block cache)
- **Scope**: FAT32/ext2/simple FS implementation
- **Impact**: Structured file storage and retrieval

#### **VFS Layer** (Issue #65)
- **Dependencies**: Issue #64 (filesystem library)
- **Scope**: Virtual File System abstraction layer
- **Impact**: Uniform interface for different filesystems

#### **File I/O Syscalls** (Issue #60)
- ✅ **Status**: Ready to implement (file descriptors complete, waiting on VFS)
- **Scope**: open/read/write/lseek/close and directory operations
- **Impact**: Userspace file access for loading WAD files

### **Phase 5: Graphics & Input**
Visual output and user interaction:

#### **Userspace Graphics Interface** (Issue #31)
- ✅ **Status**: Ready to implement (file descriptors complete)
- **Scope**: Framebuffer interface, double buffering, palette control
- **Requirements**: 320×200 paletted or 640×480 8/32-bit modes for Doom
- **Impact**: Visual output for games and applications

#### **Input Subsystem** (Issue #32)
- ✅ **Status**: Ready to implement (file descriptors complete)
- **Scope**: Userspace keyboard/mouse interface, event queue system
- **Requirements**: Character device or event queue using PS/2 driver
- **Impact**: User interaction and game controls

#### **Audio Subsystem** (Issue #33)
- ✅ **Status**: Ready to implement (file descriptors complete)
- **Scope**: PCM output, mixer/stream syscalls, timer-driven audio
- **Requirements**: 8-bit/16-bit audio buffers for Doom sound
- **Impact**: Game audio and sound effects

### **Phase 6: Toolchain and Build Flow**
Development environment for building applications:

#### **Cross-Compiler Toolchain** (Issue #29)
- **Scope**: binutils + GCC/Clang targeting meniOS userland ABI
- **Impact**: Compiling applications for meniOS

#### **C Runtime and libc**
- **Dependencies**: Threading support (Issues #109, #110)
- **Scope**: crt0, libc subset, dynamic vs static linking decisions
- **Impact**: Standard library support for applications

#### **Userspace SDK**
- **Dependencies**: Cross-compiler toolchain
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

### **Short Term (3-6 months)**
- ✅ Foundation complete!
- Complete Phase 1: Process & I/O Management (#96, #89, #93)
- Begin Phase 2: Threading Support (#108, #109)

### **Medium Term (6-12 months)**
- Complete threading infrastructure (#108-#113)
- Implement file system support (#62-#65, #60)
- Basic graphics and input (#31, #32)

### **Long Term (12+ months)**
- Audio subsystem (#33)
- Cross-compiler toolchain (#29)
- Full userspace SDK
- **Doom port and integration**

## 🚀 **Immediate Next Steps**

**Ready to implement now** (no blocking dependencies):
1. **#89** - Memory mapping syscalls (mmap/munmap)
2. **#96** - File descriptor management
3. **#108** - Kernel threading infrastructure

**High impact for applications**:
4. **#93** - Fork/exec process creation (after #96)
5. **#109** - pthread API (after #108)
6. **#102** - Pipes (after #96)

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