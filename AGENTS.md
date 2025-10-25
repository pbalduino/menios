# Repository Guidelines

## Project Structure & Module Organization
Kernel sources live in `src/` (drivers, memory, scheduler, syscall) with exported headers under `include/`. Userland libc and runtime live in `user/`, while runnable programs (e.g., shell, Doom helpers) sit in `app/`. Build products land in `build/` and the assembled disk image in `menios.hdd`/`menios.iso`. Design notes and roadmaps are in `docs/`; third-party toolchains are vendored in `vendor/`. Tests reside in `test/` alongside Unity helpers and host stubs.

## Build, Test, and Development Commands
- `make userland` – build the libc, runtime, and user programs into `build/bin/`.
- `make build` – produce the kernel, sync Limine assets, and pack the bootable HDD/ISO images.
- `make run` – boot the built image in QEMU using the project default flags.
- `make test` – compile and execute Unity-based host tests; fails fast on regressions.
- `make check` – run `cppcheck` against the kernel sources for static analysis.
Set cross-tool overrides when needed: `export MENIOS_HOST_CC=/opt/cross/bin/x86_64-elf-gcc` and `export MENIOS_CROSS_PREFIX=x86_64-elf`.

## Coding Style & Naming Conventions
Follow the two-space indentation rule across C and assembly; never commit tabs. Keep lines under 120 characters and place opening braces on the same line as declarations. Always bracket control-flow bodies, even single statements. Prefer descriptive snake_case for functions, variables, and files (`init_page_tables`, `kmalloc_stats.c`). Header guards must follow the location-based prefixes (`MENIOS_INCLUDE_KERNEL_*` for `include/kernel`, `MENIOS_*` elsewhere). Run `make check` before pushing large series—fix or suppress warnings in-tree.

## Testing Guidelines
Host tests live in `test/test_*.c` and use Unity assertions; name helpers after the feature under test (e.g., `test_heap_virtual.c`). Ensure new functionality ships with tests or updates existing cases. Run `make test` locally; the suite builds each test with kernel stubs and drops binaries in place. Boot the image with `make run` (or on hardware) when touching kernel low-level code, IPC, or filesystems to confirm no runtime regressions.

## Commit & Pull Request Guidelines
Author commits in present tense with a concise (<72 char) subject and an explanatory body covering what changes and why (e.g., `Fix buffer overflow in vsprintk`). Reference related issues or tasks using `#123` syntax and update `tasks.json` to reflect progress since it is the source of truth. Squash fix-ups before submitting. Pull requests should include a clear summary, testing notes, screenshots for UI-facing shells if relevant, and links to roadmap items being advanced. Flag breaking changes or tooling updates explicitly and update documentation (README, docs/) when behavior shifts.
