# Road to Doom on meniOS

Below is a high-level checklist of the infrastructure we still need before a vanilla Doom port can enter userland. We will break each bullet into concrete tasks later.

## Toolchain and Build Flow
- Cross-compiler that targets meniOS userland ABI (binutils + GCC/Clang).
- C runtime/startup for user processes (`crt0`, `libc` subset, dynamic vs static decision).
- Userspace SDK (headers, linker scripts, build system) so Doom and libraries compile reproducibly.

## Executable Loading & Process Model
- ELF loader for user programs (at least static ELF64).
- Per-process virtual memory layout (code/data/bss/heap/stack) and paging isolation.
- Syscall ABI and dispatcher (context switch, user ↔ kernel transition, return path, error reporting).
- Process lifecycle management (fork/exec or spawn semantics, signals/termination, wait/join).

## Memory Management
- Demand paging / lazy allocation strategy compatible with Doom’s footprint (~8–16 MiB).
- Userspace heap allocator (sbrk/munmap or mmap support) exposed through libc.
- Guard pages and copy-on-write (optional, but helps stability when porting large apps).

## Scheduling & Timing
- Preemptive scheduler with userland time slices and per‑process accounting.
- High-resolution timer services (for Doom’s tickrate) exposed via syscalls.
- Sleep/yield primitives for user threads.

## Filesystem and Storage
- Block device driver (AHCI/ATA or a RAM-backed disk) with DMA where possible.
- Filesystem implementation (FAT/ext2/ISO9660) accessible from userland.
- VFS/syscalls for open/read/write/lseek/close and directory traversal.
- Loader support for bringing Doom WAD assets from disk to user space.

## Input Subsystem
- Userspace keyboard interface (character device or event queue) using the new PS/2 driver.
- Mouse driver with relative motion buttons (PS/2 or USB HID) and userland API.
- Optional gamepad layer (nice-to-have, can be HID‑based).

## Graphics Output
- Userspace-accessible framebuffer or accelerated blitter (Doom needs 320×200 paletted or 640×480 8/32‑bit).
- Double-buffered swap or page-flip syscall to avoid tearing.
- Palette control and optional scaling (software scaler or hardware mode change).

## Audio
- PCM output driver (e.g., AC97/HDA or a simple PC speaker/PWM fallback).
- Mixer/stream syscall so Doom can push 8‑bit/16‑bit audio buffers.
- Timer-driven audio callback or asynchronous queue.

## Networking (Optional)
- Basic IP stack and UDP sockets if we ever want Doom multiplayer.

## Debugging & Diagnostics
- Userspace debugger support or at least robust crash dumps.
- Logging/syslog interface so Doom can report errors without crashing the kernel.
- Profiling hooks (timers/perf counters) to tune rendering/audio performance.

## Packaging & Deployment
- Userspace loader/launcher program (shell or menu) that can start Doom and manage arguments (e.g., `doom -file wad.wad`).
- Asset installation strategy (initrd, filesystem image, package manager).
- Documentation for porting Doom (toolchain usage, system requirements, known limitations).

## Stretch Goals
- SDL-like compatibility layer for other ports.
- Dynamic linking support to reuse shared libs (e.g., `libSDL`, `libm`).
- Savegame support using filesystem APIs and possible RTC integration for timestamps.

