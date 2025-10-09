# MeniOS

> Current release: **0.0.4**

<img alt="image" src="https://github.com/user-attachments/assets/90634816-da18-4e3c-8132-bba2ea291940">

<a rel="me" href="https://bolha.us/@p_balduino">Mastodon</a>

A hobby operating system kernel written in C and Assembly, targeting x86-64 architecture. The ultimate goal is to run Doom in userland! 🎯

If you are tracking the steps toward a usable shell, see
[Road to Shell Readiness](docs/road/road_to_shell.md) for the current checklist.

## Current Status

MeniOS has made significant progress with core kernel functionality now solidly implemented. The system boots with Limine bootloader and provides:

### ✅ **Completed Core Infrastructure**
- **✅ Memory Management**: Physical memory mapping, virtual memory allocation, and kernel heap management (Issues #35, #57)
- **✅ Process Scheduling**: Preemptive userland scheduler with kernel threads and time slicing (Issue #34)
- **✅ Synchronization**: COMPLETE! Blocking mutexes, condition variables, semaphores, and read-write locks with scheduler integration (Issues #36, #37, #39, #40) 🎉
- **Console System**: ANSI escape sequence support with scrolling and color output
- **Input/Output**: PS/2 keyboard driver with buffered input
- **Debugging**: Page fault and GPF handlers for system diagnostics
- **Testing**: Unit test framework using Unity for kernel components
- **Privilege Setup**: Ring 3 GDT selectors, 64-bit TSS, and user-mode entry trampoline
- **Syscalls**: INT 0x80 dispatcher with initial `write(1, …)` and `exit(status)` support
- **User Demo**: Kernel launches Ring 3 thread with ELF loader exercising full syscall path
- **Memory Protection**: Kernel/user separation with per-process page tables

### 🚧 **Next Major Milestones**
With the strong foundation now in place, the next high-priority developments are:

1. **File Descriptor Management** (Issue #96) - Foundation for all I/O operations
2. **Memory Mapping Syscalls** (Issue #89) - mmap/munmap for userspace allocators
3. **Fork/Exec Process Creation** (Issue #93) - Full process lifecycle management
4. **Threading Support** (Issues #108-#113) - Complete multithreading infrastructure
5. **Advanced IPC** (Issues #102-#107) - Pipes, signals, shared memory, and microkernel IPC

### 🆕 **Threading Support Added**
A complete threading roadmap has been designed with 6 new issues:
- **#108**: Kernel threading infrastructure
- **#109**: pthread API and POSIX threading support
- **#110**: Thread-safe C library (libc)
- **#111**: Advanced pthread synchronization primitives
- **#112**: Thread debugging and profiling support
- **#113**: Thread-aware system calls and kernel integration

## Quick Start

### Prerequisites

**Linux:**
- gcc
- ld
- make
- qemu

**MacOS:**
- Docker
- make
- qemu

### Building and Running

```bash
make build run
```

This will build the kernel, create a bootable image, and launch it in QEMU.
All generated artifacts now live under `build/` (`build/bin` for boot assets, `build/obj` for intermediates), keeping the repository tree clean. When the kernel reaches `halt()` the QEMU instance exits automatically via the debug-exit device, so `make run` returns to your shell without manual intervention. The default `QEMU_OPTS` wire an AHCI controller (`-device ahci`) with the disk attached to `ahci.0`, ensuring the kernel exercises its SATA/AHCI path during every run.

### Verify the User Demo

During boot, meniOS schedules the embedded `user_demo` ELF immediately after hardware probing. The program now:

- forces the stack to grow across an 8 KiB boundary (exercising lazy stack paging),
- emits three `write(1, …)` syscalls with status messages, and
- exits with status 42 via `SYS_exit`.

Expect the log to show the `[user_demo]` messages on screen and in `com1.log`, confirming that the INT 0x80 path, lazy stack allocation, and non-zero exit codes work. If you need quieter serial output, toggle the verbose syscall traces in `src/kernel/syscall/syscall.c` (search for `serial_printf` inside `syscall_write_handler`).

## Development Progress

### ✅ **Major Milestones Completed**
- [x] **Foundation Complete**: Memory management, scheduling, and synchronization (Issues #34, #35, #36, #40, #57)
- [x] Integration with Limine bootloader v10
- [x] Physical memory mapping and management with virtual memory allocation
- [x] Kernel malloc implementation with heap management
- [x] Preemptive scheduler with kernel threads and time slicing
- [x] Mutex implementation with blocking and scheduler integration
- [x] Condition variables for advanced synchronization
- [x] ANSI console with scrolling and color support
- [x] Complete vsprintk function with all format specifiers
- [x] PS/2 keyboard driver with proper input handling
- [x] Virtual-to-physical address translation (page table walking)
- [x] Ring 3 user mode infrastructure with syscall interface
- [x] ELF loader for user programs
- [x] Per-process virtual memory with kernel/user separation
- [x] Kernel block device abstraction layer (Issue #114)
- [x] SATA/AHCI DMA block driver with interrupt completion (Issue #62)
- [x] DMA-friendly allocation helpers (Issue #115)
- [x] Global block cache for block devices (Issue #63)
- [x] Kernel VFS layer backed by FAT32 filesystem (Issue #65)
- [x] GPT-aware FAT32 filesystem mounting and file access (Issue #64)
- [x] Filesystem syscalls (`open`/`read`/`write`/`lseek`/`close`) (Issue #60)

### 🔥 **Ready to Implement** (Dependencies Met)
- [x] **File descriptor management and pipes** (Issue #96)
- [x] Stdin routed through descriptor table for interactive user input
- [x] **Memory mapping syscalls (mmap/munmap)** (Issue #89) - Enabled by completed VM work
- [x] **Kernel threading infrastructure** (Issue #108)
- [x] **Fork/exec process creation** (Issue #93) - Enabled by VM and file descriptor work

### 🚧 **In Progress & Planned**

#### **Critical Path: Toolchain (Issues #29, #192-#195)** 🚀 Major Progress!
- [x] **#192**: crt0 runtime startup code ✅ COMPLETE!
- [x] **#193**: Minimal userland libc (syscalls, strings, memory, stdio) ✅ COMPLETE!
- [ ] **#194**: Syscall ABI documentation (ready to start!)
- [ ] **#195**: Userland build system (ready to start - dependencies met!)
- [ ] **#29**: Cross-compiler toolchain integration (blocked on #194, #195)

#### **Shell Milestone (Issues #180-#188)** 🎯 8/9 Complete!
- [x] **Environment**: Environment variables (#148) ✅, seeding (#180) ✅, PATH search (#185) ✅
- [ ] **Environment**: env utility (#188) - ready now!
- [x] **Testing & Validation**: tmpfs (#181) ✅, waitpid tests (#182) ✅, line editor coverage (#184) ✅
- [x] **Utilities**: /bin tools (#183) ✅
- [x] **Utilities**: ps/kill (#187) ✅
- [x] **Pipeline Support**: Pipeline placeholders (#186) ✅

#### **Shell UX Features (Issues #197-#201)**
- [x] **Keyboard Shortcuts**: Tab completion (#197) ✅, Ctrl+A/E (#198) ✅, Ctrl+L (#200) ✅, Ctrl+R (#199) ✅
- [ ] **Mouse Support**: Selection and copy/paste (#201, depends on #143/#144)

#### **System Features**
- [ ] **Threading Support**: Complete pthread API and multithreading (Issues #109-#113) - Foundation complete (#108) ✅
- [x] **IPC - Pipes & FIFOs**: ✅ **COMPLETE!** Data structure (#206) ✅, Syscall API (#207) ✅, Shell pipelines (#208) ✅, Named FIFOs (#209) ✅
- [x] **IPC - Signals**: Bookkeeping (#210) ✅, Syscalls (#211) ✅, Delivery path (#212) ✅, Shell Ctrl+C (#213) ✅
- [ ] **IPC - Signals**: Advanced features (#214)
- [x] **IPC - Shared Memory**: ✅ **COMPLETE!** Manager (#215) ✅, Syscalls (#216) ✅, Cleanup (#217) ✅, Tests (#218) ✅, Documentation (#219) ✅ *(see docs/design/shared_memory.md)*
- [x] **IPC - Device Control**: ioctl (#220) ✅ **COMPLETE**
- [ ] **IPC - Other**: Unix domain sockets (#105), Microkernel IPC (#106-#107), Fast syscalls (#221)
- [x] **Filesystem - I/O Scheduler**: Elevator I/O scheduler (#205) ✅ COMPLETE
- [ ] **Filesystem - Write Support**: FAT32 write support (#189)
- [ ] **Networking**: Complete TCP/IP stack (Issues #67-#73)
- [ ] **Graphics**: Framebuffer interface and input subsystem (Issues #31-#33)

#### **Native Compilation (Long Term, Issues #190-#191, #196)**
- [ ] **TCC Port**: Tiny C Compiler for meniOS (#190)
- [ ] **binutils Port**: Assembler and linker (#191)
- [ ] **Fish Shell**: Research modern shell porting (#196)

### Road to Doom 🎮

**Foundation ✅ COMPLETE**: The core kernel infrastructure needed for userspace applications is now solid!

Remaining major components for Doom:
- **File System**: VFS layer, write support, file I/O syscalls
- **Graphics**: Framebuffer interface, double buffering, palette control
- **Input**: Userspace keyboard/mouse drivers and event system
- **Audio**: PCM output, mixing, streaming syscalls
- **Toolchain**: Cross-compiler, libc subset, build system

See [`road_to_doom.md`](docs/road/road_to_doom.md) for the complete roadmap and [`tasks.json`](tasks.json) for detailed task tracking.

**📊 Progress Assessment**: With **47 major foundation issues completed** 🎉🎉 and ~65 open issues, meniOS is making phenomenal progress! Major recent completions include:
- ✅ **Toolchain**: crt0 (#192), libc (#193) - 2/8 complete in GCC milestone!
- ✅ **Synchronization**: All primitives complete! (#36, #37, #39, #40) - 4/4 done!
- ✅ **Shell Milestone**: Now tracking 31 issues comprehensively - 24/31 complete (77.4%)! 🎉
- ✅ **Shell UX**: Tab completion (#197) ✅, History (#156) ✅, Ctrl+A/E (#198) ✅, Ctrl+L (#200) ✅, Ctrl+R (#199) ✅, pwd prompt (#222) ✅!
- ✅ **Threading Foundation**: Kernel threading (#108) - ready for pthread!
- ✅ **IPC - Pipes & FIFOs**: **COMPLETE!** All 5 issues done (#102, #206-#209) - 5/5! 🎉
- ✅ **IPC - Signals**: Delivery path working! (#103, #210-#213) - 5/6 complete! 🎉
- ✅ **IPC - Shared Memory**: **COMPLETE!** All 5 issues done (#215-#219) - 5/5! 🎉🎉🎉
- ✅ **Performance**: I/O scheduler (#205)
- ✅ **Bug Fixes**: Pipeline hang bug (#203) ✅ fixed!

The critical path forward is completing the **toolchain** (#194, #195, #29), which is just 3 issues away from enabling standard C development for all userland applications!

**🎯 Recent Momentum** (43 issues closed recently!):
- 🛠️ **Toolchain** (2/8 in GCC milestone): crt0 ✅, libc ✅ → just need: ABI docs, build system, integration
- 🔌 **IPC** (major progress!): Pipes & FIFOs ✅✅✅✅✅ **COMPLETE!** (#102, #206-#209), Signals #103, #210-#213 ✅ (Ctrl+C working; advanced next), Shared Memory ✅✅✅✅✅ **COMPLETE!** (#215-#219), ioctl #220 ✅ **COMPLETE**, fast syscalls (#221)
- 🐚 **Shell** (24/31 in Mosh milestone - 77.4%!): Tab completion ✅, History ✅, Ctrl+A/E ✅, Ctrl+L ✅, Ctrl+R ✅, Ctrl+C ✅, I/O redirection ✅, pipelines ✅, &&/|| operators ✅, ps/kill ✅, getcwd/chdir ✅, /bin/env ✅, pwd prompt ✅!
- 🧵 **Threading** (1/6 complete): Kernel threading ✅, pthread API ready!
- 🎨 **UX** (Almost done!): Only mouse (#201) left!
- 🐛 **Bugs** (1 remaining): Just /dev/zero EOF bug (#202)

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
│  │ Process Mgmt ✅ │  │ Memory Mgmt ✅  │  │ I/O Subsys   │ │
│  │ • Scheduler ✅  │  │ • Virtual Mem ✅│  │ • Console ✅ │ │
│  │ • Kernel Threads│  │ • Physical Mem ✅│  │ • PS/2 Input │ │
│  │ • Sync Prims ✅ │  │ • Page Tables ✅│  │ • Framebuffer│ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
│                              │                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │ Debug/Diag      │  │ File System     │  │ Hardware     │ │
│  │ • Page Faults ✅│  │ • VFS (Planned) │  │ • Interrupts │ │
│  │ • GPF Handler ✅│  │ • Block Drivers │  │ • Timers     │ │
│  │ • Unit Tests ✅ │  │ • File I/O      │  │ • Hardware   │ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
│                                                             │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │            🆕 Threading Support (Planned)               │ │
│  │  • Kernel threading infrastructure (#108)              │ │
│  │  • pthread API (#109) • Thread-safe libc (#110)       │ │
│  │  • Advanced synchronization (#111) • Debugging (#112) │ │
│  └─────────────────────────────────────────────────────────┘ │
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
- **Filesystem**: Read-only FAT32 and tmpfs support exist; write support and broader FS coverage are still TODO (Issues #60, #62-#65).
- **Limited hardware support**: Only basic PS/2 keyboard, VGA framebuffer
- **No network stack**: No networking capabilities (Issues #67-#73)
- **Graphics**: Basic framebuffer, no hardware acceleration
- **Audio**: No audio subsystem implemented yet

### Active Development Areas
- **Threading**: Complete multithreading support in development (Issues #108-#113)
- **IPC**: Advanced inter-process communication planned (Issues #102-#107)
- **File I/O**: File descriptor management and filesystem support (Issues #96, #60, #62-#65)
- **Process Management**: Fork/exec and full process lifecycle (Issue #93)

### Testing Environment
- **QEMU only**: Primary testing on QEMU emulator, real hardware testing limited
- **x86-64 focus**: No support for other architectures planned currently
- **Development tools**: Requires cross-compilation toolchain for full development

## Project Structure

- **`src/`** - Kernel source code (C and Assembly)
- **`include/`** - Header files
- **`tests/`** - Unit tests using Unity framework
- **`build/`** - Build artifacts and bootloader assets
- **`docs/`** - Architecture documentation and design decisions
- **`tasks.json`** - Detailed task tracking with GitHub issue integration
- **`docs/road/road_to_doom.md`** - Comprehensive roadmap for userland Doom support
- **`docs/issue_dependencies.dot/.png`** - Visual dependency chart of all issues
- **`docs/ISSUE_DEPENDENCY_ANALYSIS.md`** - Detailed dependency analysis and implementation strategy
- **`docs/MILESTONES.md`** - Milestone tracking (Mosh, GCC, Doom)

## Contributing

We welcome contributions from developers of all skill levels! 🚀

- **New Contributors**: Start with our [Contributing Guide](CONTRIBUTING.md) for a complete development workflow
- **Find Tasks**: Check [GitHub Issues](https://github.com/pbalduino/menios/issues) or browse [`tasks.json`](tasks.json) for detailed task tracking
- **High Priority - Ready Now** (7 issues ready to start!):
  - **Toolchain** (critical): #194 (ABI docs), #195 (build system - deps met!)
  - **IPC foundations**: #213 (shell Ctrl+C) ✅, #218 (shared mem tests - cleanup done!)
  - **Shell polish**: #188 (env - deps met!), #198 (Ctrl+A/E), #200 (Ctrl+L)
  - **Performance**: #221 (fast syscalls - 3-5x speedup!)
- **Critical Path**: Toolchain issues (#192→#193→#195→#29) are the most important for enabling all future development
- **Report Issues**: Use our issue templates to report bugs or request features
- **Security Issues**: Please review our [Security Policy](SECURITY.md) for responsible disclosure
- **Code Style**: Follow the guidelines in [`CODING.md`](CODING.md)
- **Community**: Read our [Code of Conduct](CODE_OF_CONDUCT.md)

Whether you're interested in kernel development, want to learn about operating systems, or just want to help us reach the goal of running Doom in userland, there's a place for you in the meniOS community!

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Copyright (c) 2020-2025 Plínio Balduino**

## References
  - Intel® 64 and IA-32 Architectures Software Developer's Manual Combined Volumes: 1, 2A, 2B, 2C, 2D, 3A, 3B, 3C, 3D, and 4: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
  - PIC:  https://pdos.csail.mit.edu/6.828/2014/readings/hardware/8259A.pdf
          http://www.brokenthorn.com/Resources/OSDevPic.html
  - APIC: http://web.archive.org/web/20070112195752/http://developer.intel.com/design/pentium/datashts/24201606.pdf
  - ATA:  http://learnitonweb.com/2020/05/22/12-developing-an-operating-system-tutorial-episode-6-ata-pio-driver-osdev/
          http://www.t13.org/Documents/UploadedDocuments/docs2016/di529r14-ATAATAPI_Command_Set_-_4.pdf p.74
  - ASM:  https://bitismyth.wordpress.com/assembly-bunker/
  - Mem:  https://arjunsreedharan.org/post/148675821737/memory-allocators-101-write-a-simple-memory
  - AMD:  https://developer.amd.com/resources/developer-guides-manuals/
          https://www.amd.com/system/files/TechDocs/48751_16h_bkdg.pdf
  - Limine Protocol: https://codeberg.org/Limine/limine-protocol/src/branch/trunk/PROTOCOL.md

![image](https://user-images.githubusercontent.com/32979/212723683-73387eaf-4a48-4193-83b6-5ec155360a50.png)
