# MeniOS

<img alt="MeniOS screenshot" src="https://github.com/user-attachments/assets/90634816-da18-4e3c-8132-bba2ea291940" width="640">

> A hobby x86-64 operating system written in C and Assembly. Doom runs in userland today; the long-term target is reliable boots on real hardware.

## Overview

- Limine-based boot, SMP-ready kernel, and uACPI hardware discovery.
- Preemptive scheduler with fork/exec/wait, signals, shared memory, and fast syscalls.
- Virtual file system with FAT32 read/write, block cache, and `/dev` nodes (`null`, `zero`, `tty0`, `fb0`).
- Mosh interactive shell (history, completion, job control) plus a small `/bin`.
- doomgeneric port bundled as a userland game target.
- Vendored [Tiny C Compiler (TCC) 0.9.24](https://bellard.org/tcc/) under `vendor/tcc-0.9.24/`.
- Vendored [GNU binutils 2.45](https://www.gnu.org/software/binutils/) under `vendor/binutils-2.45/` for native assembler/linker work.

## Goals

- ✅ Run classic Doom in userland
- 🚀 Bring-up on real hardware (current long-term objective)

## Quick Start

### Prerequisites

| Platform | Requirements |
|----------|--------------|
| Linux    | `gcc`, `make`, `ld`, `qemu-system-x86_64` |
| macOS    | `docker`, `make`, `qemu-system-x86_64`, optional `x86_64-elf` toolchain via Homebrew |

An `x86_64-elf` cross-compiler is preferred; the build falls back to the host compiler when unavailable.

### Build & Run

```bash
make userland    # build libc and user programs only
make build       # build kernel + disk image (runs userland build when needed)
make run         # launch meniOS in QEMU
```

To point at a custom toolchain:

```bash
export MENIOS_HOST_CC=/opt/cross/bin/x86_64-elf-gcc
export MENIOS_CROSS_PREFIX=x86_64-elf   # default prefix
```

## Userland Tools

MeniOS ships with a growing collection of userland utilities in `/bin`:

### Shell & Core Utilities
- **mosh** — Interactive shell with history, tab completion, and job control
- **cat**, **echo**, **env**, **true**, **false** — Standard POSIX utilities
- **touch** — Create files and update timestamps
- **realpath** — Canonicalize file paths
- **head** — Display first lines of files (#374 - planned)
- **stat** — Display detailed file metadata (validated)
- **shutdown** — Clean ACPI power-off (#372 - planned)

### Development Tools (binutils 2.45) ✅
- **as** — GNU assembler (x86-64)
- **ld** — GNU linker
- **objdump** — Object file disassembler and analyzer
- **nm** — Symbol table viewer
- **ar** — Archive creator and manager
- **ranlib** — Archive index generator
- **objcopy** — Binary format translator
- **strip** — Binary symbol stripper
- **strings** — Extract ASCII strings from binaries
- **size** — Section size analyzer
- **readelf** — ELF header and section viewer

All binutils tools fully operational on meniOS!

### Example Workflow

```bash
# In meniOS shell (/bin/mosh)
cd /HOME

# Assemble, link, and inspect a binary
as --64 -o hello.o HELLO.S
ld -o hello hello.o
objdump -d hello
nm hello

# Create a static library
ar rcs libhello.a hello.o
ranlib libhello.a
nm libhello.a

# Analyze binaries
size hello
readelf -h hello
strings hello
```

The complete binutils 2.45 suite is fully operational on meniOS with native metadata support on tmpfs and FAT32. Core tools, including `ar` and `ranlib`, now preserve permissions when working in `/tmp` or FAT-based paths; read-only pseudo filesystems (`/dev`, `/proc`) continue to refuse metadata changes by design.

**Implementation Details:**
- Native syscalls: `chmod`/`fchmod`/`utime` implemented across tmpfs and FAT32 (pseudo-fs remain read-only and ignore metadata changes)
- FAT32 stat metadata now surfaces DOS attributes and timestamp fields (archived/hidden/system flags preserved)
- Host harness: Forwards metadata operations to host OS for cross-platform testing
- Printf formatting: Dynamic field width support (`*`) enables proper table display in `size`

**Verification:** Complete native validation sweep performed on 2025-10-29 confirmed all 11 tools working correctly.

See [issue #191 (CLOSED)](https://github.com/pbalduino/menios/issues/191) for the complete binutils integration documentation.

## Documentation

- [Roadmaps](docs/road/) — milestone breakdowns for shell, buddy allocator, GCC toolchain, Doom integration, and more.
- [docs/tools.md](docs/tools.md) — overview of the Menios toolchain wrapper scripts.
- [docs/MILESTONES.md](docs/MILESTONES.md) — high-level progress tracker.
- [scheduler_issues.md](scheduler_issues.md) — notes on ready-queue redesign.
- [CONTRIBUTING.md](CONTRIBUTING.md) & [CODING.md](CODING.md) — contribution workflow and style guide.

## Repository Layout

```
app/        user programs (doom, shell utilities, demos)
include/    public kernel and libc headers
src/        kernel source (arch, drivers, subsystems)
user/       libc, crt, and test harnesses
docs/       design references and milestone plans
tools/      build helpers and automation scripts
```

## Contributing

Bug reports, documentation updates, and code patches are welcome. Start with the issues tagged `good first issue` or `nice to have`, read the contributing guide, and follow the coding standards. Please also review the [Code of Conduct](CODE_OF_CONDUCT.md) and [Security Policy](SECURITY.md) before submitting changes.

## License

MeniOS is released under the MIT License. See [LICENSE](LICENSE) for details.

## Acknowledgments

Key third-party components:

- [Limine](https://codeberg.org/Limine/Limine) — modern x86-64 bootloader.
- [uACPI](https://github.com/uACPI/uACPI) — ACPI interpreter and tables.
- [doomgeneric](https://github.com/ozkl/doomgeneric) — portable Doom engine interface.

Special thanks to the broader OSDev community and everyone sharing documentation and tooling that make projects like this possible.

![MeniOS artwork](https://user-images.githubusercontent.com/32979/212723683-73387eaf-4a48-4193-83b6-5ec155360a50.png)
