# MeniOS

> Current release: **0.1.0**

<img alt="image" src="https://github.com/user-attachments/assets/90634816-da18-4e3c-8132-bba2ea291940">

<a rel="me" href="https://bolha.us/@p_balduino">Mastodon</a>

A hobby operating system kernel written in C and Assembly, targeting x86-64 architecture. The ultimate goal is to run Doom in userland! 🎯

## 🎉 Release v0.1.0

Version 0.1.0 is the first public milestone where meniOS boots straight into a fully interactive shell experience. Highlights of this release:

- **Mosh shell milestone complete**: init now supervises `/bin/mosh`, which delivers a polished prompt, command history, tab completion, reverse search, Ctrl shortcuts, and directory-aware `cd` UX
- **Robust command execution pipeline**: fork/exec/wait, pipes, redirection (stdin/stdout/stderr, append, fd duplication), logical operators, and background job control (`jobs`, `bg`, `fg`, Ctrl+Z) all work end-to-end
- **Utility toolbox**: core `/bin` programs (`echo`, `cat`, `env`, `true`, `false`, `ls`, `kill`, `ps`, `mem`) ship in-tree
- **Signal integration**: Ctrl+C cleanly terminates foreground jobs while preserving the supervising init loop
- **Regression coverage**: expanded Unity test suites exercise the line editor, waitpid edge cases, and shell execution paths

This release marks meniOS's transition from kernel experiments to an OS you can boot, explore, and script.

If you are tracking the steps toward a usable shell, see [Road to Shell Readiness](docs/road/road_to_shell.md) for the current checklist.

## Current Status

**📊 Progress**: 69/91 issues complete (75.8%)

MeniOS has made significant progress with core kernel functionality now solidly implemented. The system boots with Limine bootloader and provides a complete interactive shell environment.

### ✅ **Completed Milestones**

| Milestone | Status | Issues | Progress |
|-----------|--------|--------|----------|
| **🐚 Mosh (Shell)** | ✅ COMPLETE | 27/27 | 100% |
| **🧮 Buddy Allocator** | ✅ COMPLETE | 20/20 | 100% |
| **🔧 GCC (Toolchain)** | 🚀 Phase 4 Ready | 5/8 | 62.5% |
| **🎮 Doom (Full OS)** | 🟢 Active | 11/26 | 42.3% |

### 🎉 **Recent Major Achievements** (2025-10-17)

1. **FAT32 Write Support** (#189) ✅ **100% COMPLETE**
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

```
┌─────────────────────────────────────────────────────────────┐
│                    USERLAND (In Development)                │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │
│  │  Doom   │  │ Shell   │  │ Games   │  │  Applications   │ │
│  └─────────┘  └─────────┘  └─────────┘  └─────────────────┘ │
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
                              │ Hardware Abstraction
┌─────────────────────────────┴───────────────────────────────┐
│                         HARDWARE                           │
│    CPU    │    RAM    │   Storage   │  Graphics  │  Input   │
│   x86-64  │   4GB+    │    Disk     │    VGA     │   PS/2   │
└─────────────────────────────────────────────────────────────┘
```

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

## References

- **Intel® 64 and IA-32 Architectures Software Developer's Manual**: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **PIC**: https://pdos.csail.mit.edu/6.828/2014/readings/hardware/8259A.pdf
- **APIC**: http://web.archive.org/web/20070112195752/http://developer.intel.com/design/pentium/datashts/24201606.pdf
- **ATA**: http://learnitonweb.com/2020/05/22/12-developing-an-operating-system-tutorial-episode-6-ata-pio-driver-osdev/
- **Limine Protocol**: https://codeberg.org/Limine/limine-protocol/src/branch/trunk/PROTOCOL.md

![image](https://user-images.githubusercontent.com/32979/212723683-73387eaf-4a48-4193-83b6-5ec155360a50.png)
