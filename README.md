# MeniOS

> Current release: **0.0.4**

<img alt="image" src="https://github.com/user-attachments/assets/90634816-da18-4e3c-8132-bba2ea291940">

<a rel="me" href="https://bolha.us/@p_balduino">Mastodon</a>

A hobby operating system kernel written in C and Assembly, targeting x86-64 architecture. The ultimate goal is to run Doom in userland!

## Current Status

MeniOS now boots via the Limine bootloader, initializes the x86-64 platform, and brings up a fully interactive kernel with userland processes:

### Completed Core Infrastructure
- **Memory & Protection**: Physical memory discovery, virtual memory manager, kernel heap, per-process page tables, copy-on-write fork, lazy stack growth, and `mmap`/`munmap` (Issues #34, #35, #57, #89, #93)
- **Scheduling & Processes**: Preemptive kernel scheduler with kernel threads, sleep/yield, priority classes, fork/exec lifecycle, and Ring 3 entry trampoline (Issues #34, #93, #108)
- **Syscalls & Descriptors**: INT 0x80 dispatcher covering `read`, `write`, `open`, `close`, `lseek`, `mmap`, `munmap`, `pipe`, `dup`, `dup2`, `fcntl`, `yield`, `sleep`, and `exit`; per-process descriptor tables with CLOEXEC, stdin ring buffer, and serial/framebuffer streams (Issues #60, #89, #96, #102)
- **Storage & VFS**: PCI/AHCI DMA driver, global block cache, GPT discovery, FAT32 filesystem driver, and VFS namespace mounted at `/` for userland access (Issues #62-#65, #114-#115)
- **IPC & Device I/O**: Anonymous pipes, ANSI console with scrollback and color, PS/2 keyboard input routed into stdin with `/dev/input/kbd` events, serial logging, framebuffer console, and synchronization primitives (Issues #36, #37, #39, #40, #102)
- **Userland & Diagnostics**: ELF loader, multi-process user demo that exercises pipes and filesystem reads, SATA/FAT32 directory listing, Unity-based regression tests, page fault and GPF handlers for debugging

### Next Major Milestones
1. **pthread API & libc hardening** (Issues #109, #110) – expose kernel threads to user programs with a POSIX surface
2. **Thread-aware syscalls & tooling** (Issues #112, #113) – scheduler introspection, thread IDs, and blocking semantics across the syscall suite
3. **Signals & timers** (Issue #103; timer calibration #101 complete) – UNIX signal delivery, masking, and timer facilities for process control
4. **Shared memory & IPC expansion** (Issues #104-#107) – shared regions, message queues, and futex-style primitives
5. **Userspace device interfaces** (Issues #33, #61) – input event queues, audio streaming, and filesystem write support for Doom assets now that the framebuffer protocol (`SYS_FB_GETINFO`/`SYS_FB_MAP`/`SYS_FB_FLIP`) is in place

### Threading Roadmap
- **#108** (Done): Kernel threading infrastructure (thread control blocks, scheduler integration, stack management)
- **#109** (In progress): pthread API and POSIX semantics for userland threading
- **#110** (Planned): Thread-safe libc (malloc/stdio/errno coordination)
- **#111** (Planned): Advanced pthread synchronization primitives (barriers, reader-writer locks, robust mutexes)
- **#112** (Planned): Thread debugging and profiling utilities
- **#113** (Planned): Thread-aware system calls and kernel integration

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

During boot, meniOS schedules three priority-tier instances of the embedded `user_demo` ELF right after hardware probing. The user program now:

- touches a second stack page to prove lazy stack mapping before returning,
- creates an anonymous pipe, forks, and round-trips a payload from parent to child,
- exercises `sleep`/`yield` scheduling while printing status banners from low/normal/high priority contexts, and
- samples `/dev/input/kbd`, printing three keyboard events so you can verify the user-facing input stream, and
- terminates through `SYS_exit` once the loop completes.

On the kernel side, `user_demo_launch()` probes the SATA disk, dumps the first sector over serial, mounts the FAT32 root at `/`, and walks the top two directory levels via the VFS helpers. Check the `[user_demo]`, `user_demo_fs`, and `user_demo_launch` lines in `com1.log` to confirm that the syscall path, fork/exec, pipes, and filesystem stack are all healthy. If you need quieter serial output, toggle the verbose syscall traces in `src/kernel/syscall/syscall.c` (search for `serial_printf` inside `syscall_write_handler`).

## Development Progress

### Major Milestones Completed
- **Kernel foundation**: Memory management, scheduler, synchronization primitives, and Limine v10 boot flow (Issues #34-#40, #57)
- **Virtual memory & processes**: Copy-on-write fork/exec, per-process page tables, user-mode entry, and lazy stack paging (Issues #89, #93)
- **Syscall & descriptor stack**: File descriptors, CLOEXEC handling, `open`/`read`/`write`/`lseek`, `mmap`/`munmap`, `pipe`, `dup`/`dup2`, `fcntl`, `sleep`, and `yield` (Issues #60, #89, #96, #102)
- **Storage pipeline**: PCI/AHCI DMA driver, block cache, GPT scan, FAT32 filesystem driver, and VFS mount rooted at `/` (Issues #62-#65, #114-#115)
- **Userland integration**: ELF loader, libc syscall shims, stdin ring buffer, Unity regression tests, and comprehensive serial diagnostics

### Active Development & Near-Term Focus
- **Threading APIs**: pthread surface, thread-safe libc, and advanced synchronization (Issues #109-#111)
- **Thread observability**: Thread-aware syscalls, debugging hooks, and scheduling metrics (Issues #112-#113)
- **Signals & IPC**: UNIX signals, shared memory, pipes enhancements, and futex/message primitives (Issues #103-#107)
- **Userspace device interfaces**: Writable filesystem path, framebuffer protocol (`SYS_FB_GETINFO`/`SYS_FB_MAP`/`SYS_FB_FLIP`), input events, and audio streaming (Issues #33, #61)
- **Toolchain & SDK**: Cross-compiler, crt0, libc packaging, and build tooling for user apps (Issue #29)
- **Networking stack**: TCP/IP layers, sockets, and driver support (Issues #67-#73)

### Road to Doom

The core kernel and storage stack are online; Doom's remaining blockers are in userland infrastructure:
- Multi-threaded runtime: pthread API, thread-safe libc, and signal handling
- Rich IPC: shared memory, event queues, and synchronization primitives
- Device surfaces: framebuffer blitting, input, and audio interfaces exposed to Ring 3
- Filesystem write path: save-game and configuration support atop the existing FAT32 VFS
- Toolchain: cross-compilation and SDK to build the Doom port against meniOS headers

See [`road_to_doom.md`](road_to_doom.md) and [`tasks.json`](tasks.json) for the detailed roadmap and dependency tracking.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    USERLAND (In Development)                │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │
│  │  Doom   │  │  Shell  │  │  Games  │  │  Applications   │ │
│  └─────────┘  └─────────┘  └─────────┘  └─────────────────┘ │
│                              │                              │
│                        ┌─────────┐                          │
│                        │  libc   │  (Threading Support)     │
│                        └─────────┘                          │
└─────────────────────────────┬───────────────────────────────┘
                              │ Syscall Interface [OK]        │
┌─────────────────────────────┴───────────────────────────────┐
│                      KERNEL SPACE [OK]                      │
│  ┌─────────────────┐  ┌─────┴───────────┐  ┌──────────────┐ │
│  │ Process Mgmt    │  │ Memory Mgmt     │  │ I/O Subsys   │ │
│  │ Status: [OK]    │  │ Status: [OK]    │  │ Status: [OK] │ │
│  │ • Scheduler     │  │ • Virtual Mem   │  │ • Console    │ │
│  │ • Threads       │  │ • Physical Mem  │  │ • PS/2 Input │ │
│  │ • Sync Prims    │  │ • Page Tables   │  │ • Framebuffer│ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
│                              │                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │ Debug/Diag      │  │ File System     │  │ Hardware     │ │
│  │ Status: [OK]    │  │ Status: [OK]    │  │ Status: [WIP]│ │
│  │ • Page Faults   │  │ • VFS           │  │ • Interrupts │ │
│  │ • GPF Handler   │  │ • Block Drivers │  │ • Timers     │ │
│  │ • Unit Tests    │  │ • File I/O (RO) │  │ • Devices    │ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
│                                                             │
│  ┌────────────────────────────────────────────────────────┐ │
│  │                   Threading Roadmap                    │ │
│  │  • Kernel threading infrastructure [OK]                │ │
│  │  • pthread API and thread-safe libc [WIP]              │ │
│  │  • Advanced sync and tooling [WIP]                     │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────┬───────────────────────────────┘
                              │ Hardware Abstraction [WIP]    │
┌─────────────────────────────┴───────────────────────────────┐
│                         HARDWARE                            │
│    CPU    │    RAM    │   Storage   │  Graphics  │  Input   │
│   x86-64  │   4GB+    │    Disk     │    VGA     │   PS/2   │
└─────────────────────────────────────────────────────────────┘
```

## Known Issues and Limitations

### Current Limitations
- **Filesystem**: FAT32 stack is read-only; no create/write/unlink path yet (Issue #61)
- **Threading**: No pthread API or thread-safe libc exposed to userland (Issues #109-#111)
- **Signals & IPC**: UNIX signals and shared memory are not implemented (Issues #103-#107)
- **Device interfaces**: Userland cannot yet access audio via character devices; framebuffer mapping now lands a double-buffered staging area (Issue #33)
- **Networking**: TCP/IP stack, sockets, and drivers remain to be written (Issues #67-#73)
- **Tooling**: No official cross-compiler or SDK packaged for meniOS user apps (Issue #29)

### Active Development Areas
- Threading APIs and libc hardening (Issues #109-#113)
- Signals, shared memory, and futex/message IPC (Issues #103-#107)
- Filesystem write support and userland device access (Issues #33, #61)
- Toolchain and SDK preparation (Issue #29)
- Networking stack design (Issues #67-#73)

### Testing Environment
- **QEMU only**: Primary testing on QEMU emulator, real hardware testing limited
- **x86-64 focus**: No support for other architectures planned currently
- **Development tools**: Requires cross-compilation toolchain for full development

## Project Structure

- **`src/`** - Kernel source code (C and Assembly)
- **`include/`** - Header files
- **`test/`** - Unit tests using Unity framework
- **`build/`** - Build artifacts and bootloader assets
- **`docs/`** - Architecture documentation and design decisions
- **`tasks.json`** - Detailed task tracking with GitHub issue integration
- **`road_to_doom.md`** - Comprehensive roadmap for userland Doom support
- **`issue_dependencies.dot/.png`** - Visual dependency chart of all issues
- **`ISSUE_DEPENDENCY_ANALYSIS.md`** - Detailed dependency analysis and implementation strategy

## Contributing

We welcome contributions from developers of all skill levels!

- **New Contributors**: Start with our [Contributing Guide](CONTRIBUTING.md) for a complete development workflow
- **Find Tasks**: Check [GitHub Issues](https://github.com/pbalduino/menios/issues) or browse [`tasks.json`](tasks.json) for detailed task tracking
- **High Priority**: Issues #109 (pthread API), #110 (thread-safe libc), and #103/#104 (signals & shared memory) are ready with dependencies cleared
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
