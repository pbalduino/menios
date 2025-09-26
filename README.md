# MeniOS

<img alt="image" src="https://github.com/user-attachments/assets/90634816-da18-4e3c-8132-bba2ea291940">

<a rel="me" href="https://bolha.us/@p_balduino">Mastodon</a>

A hobby operating system kernel written in C and Assembly, targeting x86-64 architecture. The ultimate goal is to run Doom in userland! 🎯

## Current Status

MeniOS is in active development with basic kernel functionality implemented. The system boots with Limine bootloader and provides:

- **Memory Management**: Physical memory mapping, virtual memory allocation, and basic heap management
- **Console System**: ANSI escape sequence support with scrolling and color output
- **Process Management**: Kernel threads with basic scheduling (scheduler improvements ongoing)
- **Synchronization**: Basic mutex implementation (comprehensive sync primitives planned)
- **Input/Output**: PS/2 keyboard driver with buffered input
- **Debugging**: Page fault and GPF handlers for system diagnostics
- **Testing**: Unit test framework using Unity for kernel components
- **Privilege Setup**: Ring 3 GDT selectors, 64-bit TSS, and a user-mode entry trampoline ready for userland bring-up
- **Syscalls**: INT 0x80 dispatcher with initial `write(1, …)` and `exit(status)` support for user-mode stubs
- **User Demo**: Kernel launches a Ring 3 thread mapped into its own user page tables, prints via `write(1, …)` then exits through syscall 60, exercising the full syscall/scheduler path
- **ELF Support**: Minimal ELF64 loader maps binary segments into user address space; demo process now boots from an embedded user-mode ELF image
- **Memory Protection**: Kernel address space is marked supervisor-only; user mappings live in per-process page tables

### Userland Bring-Up Snapshot

With the privilege infrastructure in place, the next milestones are:

1. Finalise per-process virtual address spaces (current demo clones kernel root and maps user regions; next step is full isolation and cleanup tooling).
2. Enforce user/kernel page permissions everywhere (mark kernel pages supervisor-only, audit mappings).
3. Build out the syscall/interrupt return path for richer ABI support beyond `write`/`exit`.
4. Implement the ELF loader so user binaries can be placed into the new address space.

Progress on these steps unlocks the remaining Road to Doom tasks such as ELF loading, syscall dispatch, and user-mode tooling.

> Tip: `user_demo_launch()` now seeds a Ring 3 task that prints via `write(1, …)` and exits with syscall 60, exercising the user/syscall path during boot.

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

### Verify the User Demo

During boot, meniOS now schedules the embedded `user_demo` ELF immediately after hardware probing. You should see `Hello from user ELF via int 0x80!` both on the graphical console and in `com1.log`, confirming that the INT 0x80 syscall path and user ↔ kernel transitions are live. If you need a quieter serial log, toggle the verbose syscall traces in `src/kernel/syscall/syscall.c` (look for the `serial_printf` lines inside `syscall_write_handler`).

## Development Progress

### Completed ✅
- [x] Integration with Limine bootloader v10
- [x] Physical memory mapping and management
- [x] Virtual memory allocation system
- [x] Kernel malloc implementation
- [x] ANSI console with scrolling and color support
- [x] Complete vsprintk function with all format specifiers
- [x] PS/2 keyboard driver with proper input handling
- [x] Kernel thread scheduling improvements
- [x] Virtual-to-physical address translation (page table walking)

### In Progress 🚧
- [ ] Thread termination status propagation and join improvements
- [ ] TSC timekeeping calibration and boot time initialization
- [ ] kmalloc integration with virtual memory system and performance improvements
- [ ] Comprehensive synchronization primitives (semaphores, spinlocks, rwlocks, condition variables)
- [ ] Atomic operations and memory barriers for lock-free programming

### Road to Doom 🎮

The ultimate goal is running Doom in userland! This requires substantial infrastructure:

- **Userland Foundation**: ELF loader, syscall interface, process management
- **Memory Management**: Per-process virtual memory, demand paging, user heap
- **File System**: VFS layer, disk drivers, file I/O syscalls
- **Graphics**: Framebuffer interface, double buffering, palette control
- **Input**: Userspace keyboard/mouse drivers and event system
- **Audio**: PCM output, mixing, streaming syscalls
- **Toolchain**: Cross-compiler, libc subset, build system

See [`road_to_doom.md`](road_to_doom.md) for the complete roadmap and [`tasks.json`](tasks.json) for detailed task tracking.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        USERLAND (Future)                    │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │
│  │  Doom   │  │ Shell   │  │ Games   │  │  Applications   │ │
│  └─────────┘  └─────────┘  └─────────┘  └─────────────────┘ │
│                              │                             │
│                        ┌─────────┐                        │
│                        │  libc   │                        │
│                        └─────────┘                        │
└─────────────────────────────┬───────────────────────────────┘
                              │ Syscall Interface (Future)
┌─────────────────────────────┴───────────────────────────────┐
│                        KERNEL SPACE                        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │ Process Mgmt    │  │ Memory Mgmt     │  │ I/O Subsys   │ │
│  │ • Scheduler     │  │ • Virtual Mem   │  │ • Console    │ │
│  │ • Kernel Threads│  │ • Physical Mem  │  │ • PS/2 Input │ │
│  │ • Synchronization│ │ • Page Tables   │  │ • Framebuffer│ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
│                              │                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │ Debug/Diag      │  │ File System     │  │ Hardware     │ │
│  │ • Page Faults   │  │ • VFS (Future)  │  │ • Interrupts │ │
│  │ • GPF Handler   │  │ • Block Drivers │  │ • Timers     │ │
│  │ • Unit Tests    │  │ • File I/O      │  │ • Hardware   │ │
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
- **Kernel-only**: User mode groundwork underway (Ring 3 GDT entries + TSS in place)
- **Single-threaded userspace**: No process isolation or multi-process support
- **Limited synchronization**: Basic mutex only, no semaphores/spinlocks/rwlocks yet
- **Limited hardware support**: Only basic PS/2 keyboard, VGA framebuffer
- **No file system**: No persistent storage or file I/O capabilities
- **Basic memory management**: No demand paging or memory protection between processes
- **No network stack**: No networking capabilities

### Active Issues
- **Caret rendering**: Fixed but may need refinement for different scenarios
- **Thread synchronization**: Join operations and sleep states need alignment with scheduler
- **Timer accuracy**: TSC calibration needed for accurate timing operations
- **Memory allocation**: kmalloc needs integration with virtual memory system
- **Exception handlers**: Page fault and GPF handlers need better diagnostic output

### Testing Environment
- **QEMU only**: Primary testing on QEMU emulator, real hardware testing limited
- **x86-64 focus**: No support for other architectures planned
- **Development tools**: Requires cross-compilation toolchain for full development

## Project Structure

- **`src/`** - Kernel source code (C and Assembly)
- **`include/`** - Header files
- **`tests/`** - Unit tests using Unity framework
- **`bin/`** - Build artifacts and bootloader assets
- **`tasks.json`** - Detailed task tracking with GitHub issue integration
- **`ROAD_TO_DOOM.md`** - Comprehensive roadmap for userland Doom support
- **`CONTRIBUTING.md`** - Complete guide for contributors and development workflow
- **`SECURITY.md`** - Security policy and vulnerability reporting guidelines
- **`CODING.md`** - Coding style guidelines and standards
- **`CODE_OF_CONDUCT.md`** - Community guidelines and standards

## Contributing

We welcome contributions from developers of all skill levels! 🚀

- **New Contributors**: Start with our [Contributing Guide](CONTRIBUTING.md) for a complete development workflow
- **Find Tasks**: Check [GitHub Issues](https://github.com/pbalduino/menios/issues) or browse [`tasks.json`](tasks.json) for detailed task tracking
- **Report Issues**: Use our issue templates to report bugs or request features
- **Security Issues**: Please review our [Security Policy](SECURITY.md) for responsible disclosure
- **Code Style**: Follow the guidelines in [`CODING.md`](CODING.md)
- **Community**: Read our [Code of Conduct](CODE_OF_CONDUCT.md)

Whether you're interested in kernel development, want to learn about operating systems, or just want to help us reach the goal of running Doom in userland, there's a place for you in the meniOS community!

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Copyright (c) 2020-2025 Plínio Balduino**

## References
  - Intel® 64 and IA-32 Architectures Software Developer’s Manual Combined Volumes: 1, 2A, 2B, 2C, 2D, 3A, 3B, 3C, 3D, and 4: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
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
