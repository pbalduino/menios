# meniOS Code Structure

This document provides a quick reference for the current meniOS layout and the
conventions to follow when adding new code. The goal is to keep subsystems
isolated, make ownership of modules obvious, and ensure the build remains
reproducible.

## Top-Level Layout

```
app/          # User programs built into the image (e.g. mosh, ps, kill)
build/        # Generated artifacts (kernel, userland binaries, disk images)
docs/         # Architecture notes, roadmaps, design discussions
include/      # Public headers exported to kernel and userland
src/          # Kernel and shared libc sources
test/         # Unity-based host tests and stubs
user/         # libc implementation and runtime support objects
vendor/       # Third-party code (toolchains, Doom helper, etc.)
```

Only `build/` is expected to contain generated files; everything else should be
under version control.

## Kernel Source Tree

All kernel code lives under `src/kernel/` and is split by responsibility:

| Directory | Purpose |
|-----------|---------|
| `core/` | Early boot, halt/panic paths, service bootstrap |
| `arch/x86_64/` | Architecture-specific support (APIC, GDT, IDT, LAPIC timers, syscall entry stubs) |
| `drivers/` | Hardware drivers grouped by bus/type (`audio/`, `block/`, `bus/`, `input/`, …) |
| `console/` | Console front-ends (`console.c`, ANSI parser, serial backend) |
| `framebuffer/` | Linear framebuffer console implementation and font tables |
| `fs/` | Filesystem layer (`core/` helpers, `devfs/`, `fat32/`, `procfs/`, `tmpfs/`, `vfs/`) |
| `hw/` | Generic hardware inventory helpers |
| `input/` | Higher-level input processing (keyboard event queue, etc.) |
| `ipc/` | Shared-memory manager and related primitives |
| `mem/` | Physical and virtual memory managers, heap implementation, DMA helpers |
| `proc/` | Process lifecycle, scheduler, signals, kernel threading primitives |
| `syscall/` | Syscall dispatch table and ABI entry plumbing |
| `timer/` | HPET, PIT, LAPIC timer drivers and generic timer services |
| `user/` | ELF loader, init launcher, and helpers that bridge kernel ↔ user space |

### Headers

Headers mirror the source hierarchy under `include/`. For example:

```
include/kernel/
├── console/
├── drivers/
├── fs/
├── mem/
└── …
```

When introducing new modules add headers alongside existing ones so callers can
`#include <kernel/<subsystem>/<header>.h>` without additional include paths.

## Userland & Runtime

Userland support code is split between:

- `user/libc/` – User-mode entry glue, stdio shims, and libc-facing helpers.
- `user/crt/` – Startup/termination objects linked into every program.
- `app/` – Standalone programs that are installed into `/bin` at build time.
- `src/usermode/` – Boot-time userland payloads (`init`, `user_demo`).

## Build Output Expectations

- Object files and dependency files are written under `build/obj/**`.
- Generated binaries, disk images, and intermediate SDK artifacts live under
  `build/bin/`.
- Source directories must remain free of `.o`, `.d`, and other generated files.
- When new modules are added update the explicit lists in `Makefile` (kernel
  sources, host tests, uACPI files, user programs) so the build remains
  deterministic.

## Naming Conventions

- Functions follow either `subsystem_action()` or `verb_noun()` format.
  Examples: `scheduler_initialize()`, `buddy_alloc()`, `fs_path_resolve()`.
- File names match the module’s primary responsibility (`scheduler.c`,
  `block_cache.c`, `proc/signal.c`).
- Prefix exported helpers with the subsystem (`proc_`, `fs_`, `char_device_`)
  to make call sites self-documenting.

## Adding New Code

1. Place sources in the directory matching the subsystem (e.g., a new PCI
   driver goes under `src/kernel/drivers/bus/`).
2. Add a header beneath `include/kernel/…` so other modules can include it.
3. Extend the explicit source list in `Makefile`.
4. Keep generated artifacts in `build/` only—never commit `.o`, `.d`, or other
   build products.
5. Update relevant documentation (this file, subsystem READMEs) if the new code
   introduces a new pattern others should follow.

Refer back here when reviewing patches to ensure the layout remains consistent.
