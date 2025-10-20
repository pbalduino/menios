# MeniOS

> Current release: **0.1.666**

<img alt="image" src="https://github.com/user-attachments/assets/90634816-da18-4e3c-8132-bba2ea291940">

<a rel="me" href="https://bolha.us/@p_balduino">Mastodon</a>

A hobby operating system kernel written in C and Assembly, targeting x86-64 architecture. The ultimate goal is to run Doom in userland! 🎯

**Built with:**
- **[doomgeneric](https://github.com/ozkl/doomgeneric)** by ozkl - Platform-agnostic Doom port
- **[Limine](https://codeberg.org/Limine/Limine)** - Modern x86-64 bootloader
- **[uACPI](https://github.com/uACPI/uACPI)** - ACPI implementation

## 🎉 Release v0.1.666 - "DOOM READY"

**The Doom milestone is complete!** Version 0.1.666 delivers all infrastructure needed to run the classic 1993 Doom game in userland. This release transforms meniOS from a shell-focused OS into a gaming-capable platform with robust graphics, input, file I/O, and process management.

### 🎮 **What's New Since v0.1.0**

#### **Doom Integration (33/33 issues complete)**
- **Port Layer**: Complete [doomgeneric](https://github.com/ozkl/doomgeneric) integration via `doomgeneric_menios.c` with DG_Init/DrawFrame/GetKey/SleepMs/GetTicksMs
- **Graphics Pipeline**: `/dev/fb0` with pixel-addressable framebuffer, mmap support, and direct video RAM blitting
- **Input System**: Real keyboard events with scan codes, key press/release tracking, and modifier state
- **Build Integration**: Doom compiles automatically with `make userland` and ships in every disk image
- **Config Parsing**: scanf family (sscanf/fscanf/vfscanf) for reading game configuration files

#### **File System Enhancements**
- **FAT32 Write Support**: File creation, modification, truncation with O_CREAT/O_TRUNC/O_EXCL flags (#189, #291-#293)
- **VFS Streaming I/O**: Block cache with LRU eviction, read-ahead, write-behind for efficient asset loading (#294-#298)
- **I/O Scheduler**: Elevator-based disk I/O scheduler reduces seek latency for concurrent operations (#205)
- **File Operations**: fopen/fclose/fread/fwrite/fseek/ftell buffered I/O (#305)
- **Filesystem Helpers**: mkdir/remove/rename/unlink for save game management (#306)

#### **libc Completion**
- **String Utilities**: strdup/strndup/strcasecmp/strncasecmp for case-insensitive comparisons (#308)
- **Environment Access**: getenv/putenv/setenv/unsetenv for DOOMWADDIR detection (#307)
- **Formatted I/O**: snprintf/vsnprintf/vsprintf properly exposed, printf family now supports field widths, zero-padding, precision (#309, #324)
- **Math Library**: fabs/fabsf/fabsl for video scaling calculations (#310)
- **Input Parsing**: Complete scanf family implementation with C99 format specifiers (#304)

#### **Process & Signal Stability**
- **Shell Robustness**: Fixed all shell crash scenarios - clean Doom exit (#323), Doom crashes (#320), alarm signals (#322)
- **Process Lifecycle**: Proper waitpid() handling for abnormal child termination
- **Signal Frame Management**: Correct signal frame setup and stack management prevents overflow
- **Fast Syscalls**: Migrated from int 0x80 to syscall/sysret with proper 64-bit return values (#221)

#### **System Services**
- **Time Management**: RTC driver, nanosleep(), setitimer(), gettimeofday() for game timing (#286-#288, #290)
- **Time Conversions**: gmtime_r/mktime/strftime for timestamps and build system integration (#290)
- **Device Control**: ioctl syscall for device-specific operations (TIOCGWINSZ, FBIOGET_VSCREENINFO) (#220)
- **Shared Memory**: Complete IPC implementation with shmget/shmat/shmdt for fast data sharing (#215-#219)

#### **Shell & UX Improvements**
- **Startup Scripts**: `.moshrc` support for shell customization (#314)
- **Memory Utility**: `/bin/mem` for system memory diagnostics (#243)
- **Enhanced Utilities**: Improved error handling and robustness across all `/bin` tools

#### **Memory Management**
- **Buddy Allocator**: Production-ready buddy allocator with proper coalescing and fragmentation resistance (20/20 issues complete)
- **Userspace malloc**: Efficient heap management for large applications like Doom (#95)

### 🏆 **Milestone Achievement**

meniOS has reached **97.8% completion** across all major milestones:
- ✅ **Mosh Shell**: 27/27 (100%) - Interactive shell with job control
- ✅ **Buddy Allocator**: 20/20 (100%) - Production-ready memory management
- ✅ **Doom (Full OS)**: 33/33 (100%) - All infrastructure for gaming complete
- 🚀 **GCC Toolchain**: 7/9 (77.8%) - Native compilation ready to start

### 📦 **What's Included**

This release includes everything from v0.1.0 plus:
- Complete Doom port with build system integration
- Save game support via FAT32 writes
- Efficient asset streaming with block cache
- Robust shell that survives all game scenarios
- Production-ready graphics and input subsystems
- Comprehensive libc for userland application development

### 🎯 **Next Steps**

With the Doom milestone complete, future work focuses on:
- **Native Compilation**: TCC and binutils ports (ready to start - all dependencies met!)
- **Threading Support**: pthread API for multithreaded applications
- **Audio Subsystem**: Sound effects and music for Doom
- **Extended Features**: Mouse support, timezone database, advanced utilities

**Ready to play?** Boot meniOS and run `doom -iwad doom1.wad` to experience classic Doom on a hobby OS!

---

### 📚 **Release v0.1.0 - "SHELL READY"** (Previous Release)

The first public milestone where meniOS boots into a fully interactive shell:
- Mosh shell with command history, tab completion, reverse search
- Process management with fork/exec/wait, pipes, redirection
- Job control (bg/fg/Ctrl+Z) and signal integration (Ctrl+C)
- Core utilities: echo, cat, env, ls, kill, ps, mem

See [Road to Shell Readiness](docs/road/road_to_shell.md) for the complete shell milestone checklist.

## Current Status

**📊 Progress**: 87/89 issues complete (97.8%)

MeniOS has made significant progress with core kernel functionality now solidly implemented. The system boots with Limine bootloader and provides a complete interactive shell environment.

### ✅ **Completed Milestones**

| Milestone | Status | Issues | Progress |
|-----------|--------|--------|----------|
| **🐚 Mosh (Shell)** | ✅ COMPLETE | 27/27 | 100% |
| **🧮 Buddy Allocator** | ✅ COMPLETE | 20/20 | 100% |
| **🔧 GCC (Toolchain)** | 🚀 Phase 4 Ready | 7/9 | 77.8% |
| **🎮 Doom (Full OS)** | ✅ COMPLETE | 33/33 | 100% |

### 🎉 **Recent Major Achievements** (2025-10-20)

1. **🎮 DOOM MILESTONE COMPLETE!** ✅ **33/33 (100%)**
   - All core infrastructure for running Doom in userland complete
   - Build integration, stability fixes, and libc gaps all resolved
   - Shell robustly handles all Doom exit scenarios

2. **FAT32 Write Support** (#189) ✅ **100% COMPLETE**
   - File creation, truncation, persistence (#291, #292, #293)
   - O_CREAT, O_TRUNC, O_EXCL flags working
   - Comprehensive regression tests

2. **VFS Streaming I/O** (#294) ✅ **100% COMPLETE**
   - Block cache with LRU eviction (#295)
   - FAT32 refactored to use cache (#296)
   - Streaming read/write operations (#297)
   - Read-ahead and write-behind (#298)
   - sync()/fsync() syscalls

3. **Time Conversion Utilities** (#290) ✅ **COMPLETE**
   - gmtime_r(), mktime(), strftime()
   - Foundation for timezone support (#299)

### 🚀 **Ready to Start NOW** (Zero Dependencies!)

- **#190 - TCC Port** (Tiny C Compiler) - **FULLY UNBLOCKED!**
- **#191 - binutils Port** (Assembler and Linker) - **FULLY UNBLOCKED!**
- **#109 - pthread API** (Kernel threading complete)

**Critical Dependencies Met**:
- ✅ Buddy Allocator (100%) - Robust memory management
- ✅ FAT32 Write Support (100%) - Can write compiled binaries
- ✅ VFS Streaming I/O (100%) - Efficient file operations
- ✅ I/O Scheduler (100%) - Performance optimized
- ✅ Fast Syscalls (100%) - syscall/sysret with 64-bit returns

## Quick Start

### Prerequisites

**Linux:**
- gcc
- x86_64-elf binutils/GCC toolchain (optional but recommended)
- ld, make, qemu

**MacOS:**
- Docker, make, qemu
- x86_64-elf toolchain (via homebrew tap like `nativeos/i386-elf-toolchain`)

### Building and Running

```bash
make userland        # build libc + /bin utilities (no kernel)
make build           # build kernel + disk image (runs userland on Linux)
make run             # launch the image in QEMU
```

The separated `userland` target allows you to iterate on libc or `/bin` utilities without recompiling the kernel. `make build` assembles the kernel, regenerates the Limine assets, produces `menios.hdd` and `menios.iso`, and copies the freshly built user programs into `/bin`. When the kernel reaches `halt()` the QEMU instance exits automatically, so `make run` returns to your shell without manual intervention.

#### Using a cross compiler

The build scripts prefer an `x86_64-elf` cross toolchain when available. Set `MENIOS_HOST_CC` (and optionally `MENIOS_CROSS_PREFIX`) if your compiler lives under a different prefix:

```bash
export MENIOS_HOST_CC=/opt/cross/bin/x86_64-elf-gcc
export MENIOS_CROSS_PREFIX=x86_64-elf   # default
```

If the cross toolchain is not found the build falls back to the host compiler.

## Development Progress

### ✅ **Completed Core Infrastructure**

- **Memory Management**: Physical/virtual memory, kernel heap, buddy allocator, mmap/munmap (Issues #35, #57, #89, #245-#273)
- **Process Management**: Preemptive scheduler, kernel threads, fork/exec/wait (Issues #34, #93, #108)
- **Synchronization**: Mutexes, condition variables, semaphores, read-write locks (Issues #36, #37, #39, #40)
- **File System**: VFS layer, FAT32 read/write, streaming I/O, block cache (Issues #60, #65, #189, #294)
- **IPC**: Pipes, FIFOs, signals, shared memory, fast syscalls (Issues #102, #103, #206-#219, #221)
- **Shell**: Interactive mosh shell, job control, tab completion, history (v0.1.0 shipped!)
- **Toolchain**: Cross-compiler, crt0, libc, build system (Issues #29, #192-#195)

### 🔄 **Active Development**

#### **Native Compilation** (READY TO START!)
- **#190**: TCC port - **ZERO BLOCKERS**
- **#191**: binutils port - **ZERO BLOCKERS**
- #196: Fish shell research

#### **Threading Support** (Foundation Complete)
- ✅ #108: Kernel threading - DONE
- **#109**: pthread API - **READY NOW**
- #110: Thread-safe libc
- #111: Advanced pthread synchronization

#### **System Features**
- #33: Audio subsystem for Doom
- #214: Advanced signal features (optional)
- #226: RTC and time management
- #299: Timezone support (IANA tz database)

### 📊 **Detailed Progress by Category**

#### **Buddy Allocator** ✅ 100% COMPLETE (20/20)
- Core implementation (9/9) ✅
- Critical security fixes (3/3) ✅
- Reliability improvements (5/5) ✅
- Performance optimizations (3/3) ✅

#### **Shell (Mosh)** ✅ 100% COMPLETE (27/27)
- Interactive REPL, command execution
- Pipelines, I/O redirection
- Job control (Ctrl+Z, bg/fg, jobs)
- Tab completion, reverse search (Ctrl+R)
- Environment variables, PATH search
- /bin utilities (echo, cat, env, ps, kill, ls, mem)

#### **IPC Mechanisms**
- Pipes & FIFOs ✅ 100% (#206-#209)
- Signals ✅ 80% (#210-#213, #214 optional)
- Shared Memory ✅ 100% (#215-#219)
- Fast Syscalls ✅ 100% (#221)

#### **File System**
- VFS layer ✅ 100%
- FAT32 read/write ✅ 100%
- Streaming I/O ✅ 100%
- I/O scheduler ✅ 100%

## Architecture Overview

**Boot**: [Limine](https://codeberg.org/Limine/Limine) bootloader → meniOS kernel → userland init

```
┌─────────────────────────────────────────────────────────────┐
│                    USERLAND ✅                               │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │
│  │  Doom*  │  │ Shell   │  │ Games   │  │  Applications   │ │
│  └─────────┘  └─────────┘  └─────────┘  └─────────────────┘ │
│  *doomgeneric│              │                             │
│                              │                             │
│                        ┌─────────┐                        │
│                        │  libc   │  (Threading Support)   │
│                        └─────────┘                        │
└─────────────────────────────┬───────────────────────────────┘
                              │ Syscall Interface ✅
┌─────────────────────────────┴───────────────────────────────┐
│                      KERNEL SPACE ✅                       │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │ Process Mgmt ✅ │  │ Memory Mgmt ✅  │  │ I/O Subsys ✅│ │
│  │ • Scheduler ✅  │  │ • Buddy Alloc ✅│  │ • Console ✅ │ │
│  │ • fork/exec ✅  │  │ • Virtual Mem ✅│  │ • Block I/O ✅│ │
│  │ • Signals ✅    │  │ • mmap/munmap ✅│  │ • FAT32 R/W ✅│ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
│                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │ IPC ✅          │  │ File System ✅  │  │ Toolchain ✅ │ │
│  │ • Pipes ✅      │  │ • VFS ✅        │  │ • crt0 ✅    │ │
│  │ • FIFOs ✅      │  │ • FAT32 ✅      │  │ • libc ✅    │ │
│  │ • Shared Mem ✅ │  │ • Streaming ✅  │  │ • cross-gcc ✅│ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
└─────────────────────────────┬───────────────────────────────┘
                              │ Hardware Abstraction (uACPI*)
┌─────────────────────────────┴───────────────────────────────┐
│                    HARDWARE (x86-64)                       │
│    CPU    │    RAM    │   Storage   │  Graphics  │  Input   │
│   x86-64  │   4GB+    │    Disk     │    VGA     │   PS/2   │
└─────────────────────────────────────────────────────────────┘
                              │
                      Boot: Limine* bootloader
```

**Third-party components**: *[Limine](https://codeberg.org/Limine/Limine) bootloader, *[uACPI](https://github.com/uACPI/uACPI) ACPI implementation, *[doomgeneric](https://github.com/ozkl/doomgeneric) Doom port

## Known Issues and Limitations

### Current Limitations
- **Filesystem**: FAT32 read/write complete ✅; long filenames and directory creation remain TODO
- **Hardware**: Only basic PS/2 keyboard and VGA framebuffer
- **Networking**: No network stack yet (Issues #67-#73)
- **Audio**: No audio subsystem implemented yet (#33)

### Active Development Areas
- **Native Compilation**: TCC and binutils ports ready to start (#190, #191)
- **Threading**: pthread API ready for implementation (#109)
- **Timezone Support**: IANA tz database integration planned (#299)

### Testing Environment
- **QEMU**: Primary testing platform
- **x86-64**: Single architecture focus
- **Cross-compilation**: Recommended for development

## Project Structure

- `src/` - Kernel source code (C and Assembly)
- `include/` - Header files
- `tests/` - Unit tests using Unity framework
- `build/` - Build artifacts and bootloader assets
- `docs/` - Architecture documentation and design decisions
- `docs/road/` - Milestone roadmaps:
  - [road_to_shell.md](docs/road/road_to_shell.md) - Shell milestone (✅ v0.1.0 complete!)
  - [road_to_buddy_allocator.md](docs/road/road_to_buddy_allocator.md) - Memory management (✅ complete!)
  - [road_to_gcc.md](docs/road/road_to_gcc.md) - Toolchain development (🚀 Phase 4 ready!)
  - [road_to_doom.md](docs/road/road_to_doom.md) - Game porting requirements
- `docs/issue_dependencies.dot/.png` - Visual dependency chart
- `docs/MILESTONES.md` - Comprehensive milestone tracking

## Contributing

We welcome contributions from developers of all skill levels! 🚀

### Ready to Start (Zero Dependencies!)

1. **#190 - TCC Port** (**FULLY UNBLOCKED!**)
   - Tiny C Compiler for native compilation
   - All dependencies met: Buddy allocator ✅, FAT32 writes ✅, Streaming I/O ✅

2. **#191 - binutils Port** (**FULLY UNBLOCKED!**)
   - Assembler and linker for native toolchain
   - All dependencies met: Buddy allocator ✅, FAT32 writes ✅, Streaming I/O ✅

3. **#109 - pthread API** (READY NOW!)
   - POSIX threading for userland applications
   - Kernel threading infrastructure complete (#108 ✅)

### Recent Completions (2025-10-17)
- ✅ **FAT32 Write Support** (#189, #291-#293) - File creation and persistence
- ✅ **VFS Streaming I/O** (#294-#298) - Block cache and optimizations
- ✅ **Time Conversions** (#290) - gmtime/mktime/strftime

### Milestone Status
- **Mosh (Shell)**: ✅ 100% (27/27) - v0.1.0 shipped
- **Buddy Allocator**: ✅ 100% (20/20) - Production ready
- **FAT32 Writes**: ✅ 100% (4/4) - Complete
- **VFS Streaming I/O**: ✅ 100% (5/5) - Complete

### Get Started
- **New Contributors**: Start with our [Contributing Guide](CONTRIBUTING.md)
- **Find Tasks**: Check [GitHub Issues](https://github.com/pbalduino/menios/issues)
- **Report Issues**: Use our issue templates
- **Security**: Review our [Security Policy](SECURITY.md)
- **Code Style**: Follow guidelines in [CODING.md](CODING.md)
- **Community**: Read our [Code of Conduct](CODE_OF_CONDUCT.md)

Whether you're interested in kernel development, want to learn about operating systems, or just want to help us reach the goal of running Doom in userland, there's a place for you in the meniOS community!

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Copyright (c) 2020-2025 Plínio Balduino**

## Acknowledgments

meniOS wouldn't be possible without these excellent open-source projects:

### Third-Party Code Used

- **[doomgeneric](https://github.com/ozkl/doomgeneric)** by [@ozkl](https://github.com/ozkl) - Platform-agnostic Doom port that makes running the classic 1993 game on custom platforms possible. meniOS implements the doomgeneric interface in `app/doom/doomgeneric_menios.c`.

- **[Limine](https://codeberg.org/Limine/Limine)** by [mintsuki](https://codeberg.org/mintsuki) and contributors - Modern, feature-rich x86-64 bootloader with excellent multiboot2 protocol support. meniOS uses Limine for reliable boot and hardware initialization.

- **[uACPI](https://github.com/uACPI/uACPI)** by [@UltraOS](https://github.com/UltraOS) - Portable ACPI implementation that provides robust hardware discovery and power management. Integrated into meniOS for platform compatibility.

### Documentation & References

- **Intel® 64 and IA-32 Architectures Software Developer's Manual**: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **PIC**: https://pdos.csail.mit.edu/6.828/2014/readings/hardware/8259A.pdf
- **APIC**: http://web.archive.org/web/20070112195752/http://developer.intel.com/design/pentium/datashts/24201606.pdf
- **ATA**: http://learnitonweb.com/2020/05/22/12-developing-an-operating-system-tutorial-episode-6-ata-pio-driver-osdev/
- **Limine Protocol**: https://codeberg.org/Limine/limine-protocol/src/branch/trunk/PROTOCOL.md

**Thank you** to all the developers and maintainers who make their work freely available. Standing on the shoulders of giants! 🙏

![image](https://user-images.githubusercontent.com/32979/212723683-73387eaf-4a48-4193-83b6-5ec155360a50.png)
