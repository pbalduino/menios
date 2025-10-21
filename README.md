# MeniOS

<img alt="MeniOS screenshot" src="https://github.com/user-attachments/assets/90634816-da18-4e3c-8132-bba2ea291940" width="640">

> A hobby x86-64 operating system written in C and Assembly. Doom runs in userland today; the long-term target is reliable boots on real hardware.

## Overview

- Limine-based boot, SMP-ready kernel, and uACPI hardware discovery.
- Preemptive scheduler with fork/exec/wait, signals, shared memory, and fast syscalls.
- Virtual file system with FAT32 read/write, block cache, and `/dev` nodes (`null`, `zero`, `tty0`, `fb0`).
- Mosh interactive shell (history, completion, job control) plus a small `/bin`.
- doomgeneric port bundled as a userland game target.

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

## Documentation

- [Roadmaps](docs/road/) — milestone breakdowns for shell, buddy allocator, GCC toolchain, Doom integration, and more.
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
