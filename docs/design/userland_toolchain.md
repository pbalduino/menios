# meniOS Userland Toolchain Plan

## Scope

This document captures the integration plan for the cross-compiler toolchain (#29),
crt0 runtime (#192), minimal libc subset (#193), and the standalone userland build
system (#195). It translates the roadmap items into concrete repository changes so
we can start committing incremental progress.

## Current State

- Each user program implements its own `_start` symbol and ad-hoc syscall stubs.
- Kernel-oriented libc sources (`src/libc/*.c`) are compiled directly into the
  kernel; there is no shared library artefact for user space.
- `Makefile` builds user binaries in-place with host `gcc` and bespoke flags;
  there is no sysroot or SDK layout for external consumers.
- Syscall numbers live in `include/kernel/syscall.h`, which is not exported for
  userland consumption.
- No automatic way to stage headers/libraries that a future cross-toolchain or
  SDK can consume.

## Target Architecture

We will introduce a three-layer structure:

1. **sdk/** — generates a sysroot containing headers and static libraries:
   - `sdk/include/` mirrors the exported public headers.
   - `sdk/lib/` ships `libmeniosc.a` (the minimal libc) and `crt0.o`.
   - `sdk/bin/` provides wrapper scripts (`menios-gcc`, `menios-ld`) that point
     to either a real cross-compiler (when available) or fall back to the host
     toolchain with the correct flags while #29 is still in progress.

2. **user/crt/** — houses the canonical startup objects:
   - `crt0.S` implements the `_start` entry point, calls constructors, invokes
     `main`, and terminates via `exit`.
   - Future objects such as `crti.o`/`crtn.o` can be added without touching
     application code.

3. **user/libc/** — contains the userland C library sources:
   - `syscall.c` exposes thin wrappers for the supported syscalls.
   - `stdlib.c`, `stdio.c`, `string.c`, `ctype.c`, etc. are built by reusing the
     existing implementations in `src/libc/` where possible.
   - `errno.c` provides the TLS-like stub until threading lands (#109).

The kernel keeps using the existing objects; we will split build rules so that
common code is compiled twice (once with `-DMENIOS_KERNEL`, once for userland).

## Completed Deliverables

- ✅ Shared SDK staging (`make sdk`) exports headers, `crt0.o`, and `libmeniosc.a` with wrapper toolchain (`menios-gcc`).
- ✅ Minimal libc primitives: `memcpy`, `memmove`, `memset`, `memcmp`, and `memchr` now live in `src/libc/string.c` for both kernel and userland.
- ✅ Basic heap management: `malloc`, `free`, and `aligned_alloc` backed by anonymous `mmap` live in `user/libc/stdlib.c`.
- ✅ Core stdio support: `printf`/`fprintf`/`sprintf`, `puts`/`putchar`, and `perror` implemented in `user/libc/stdio.c`, plus stub FILE handles for `stdin`, `stdout`, and `stderr`.

## Build Flow

1. `make sdk`
   - builds `lib/libmeniosc.a` from `user/libc/*.c` plus selected sources in
     `src/libc/`.
   - assembles `user/crt/crt0.S` into `lib/crt0.o`.
   - stages include files into `build/sdk/include`.
   - generates wrapper scripts into `build/sdk/bin`:
     ```sh
     menios-gcc -> gcc -nostdlib -ffreestanding -isystem <sysroot>/include \
                   -L<sysroot>/lib -lmeniosc -static
     ```

2. `make user-apps`
   - uses the wrappers (or directly the host compiler when running in-tree) to
     build binaries inside `build/bin/bin/`.
   - apps depend only on `main`, `libmeniosc`, and exported headers.

3. `make build`
   - depends on `make sdk` and `make user-apps` so the kernel image always embeds
     binaries produced via the shared toolchain.

The wrappers unblock #29 immediately (developers can call `menios-gcc` without
waiting for a real cross compiler) and give us a drop-in location to swap once
we build a genuine `x86_64-menios-gcc`.

## Syscall ABI Exposure (#194 linkage)

While #194 remains open, we will provide interim headers under
`include/menios/syscall.h` exporting syscall numbers and inline helpers. Once the
formal ABI document is ready, the header can point to the reference doc.

## Incremental Delivery Plan

1. Introduce the directory layout, shared headers, and staged sysroot with
   wrapper scripts. Adjust one small user program (e.g., `/bin/true`) to use
   the new startup path as proof of concept.
   - ✅ `/bin/true`
   - ✅ `/bin/false`
   - ✅ `/bin/echo`
   - ✅ `/bin/cat`
   - ✅ `/bin/env`
   - ✅ `/bin/ls`
   - ✅ `/bin/kill`
   - ✅ `/bin/ps`
   - ⏳ `mosh`
2. Gradually port remaining apps; keep compatibility shims (`_start` calling
   into `main`) until everything is migrated.
   - Prioritise simple wrappers next (`echo`, `env`) before tackling
     interactive programs (`mosh`) that will want richer stdio.
3. Extend the minimal libc with additional APIs as we port utilities.
   - ✅ Memory helpers (`memcpy`, `memmove`, `memset`, `memcmp`, `memchr`).
- ✅ Basic stdio (`printf`, `fprintf`, `sprintf`, `puts`, `perror`). Host tests currently cover the memory primitives; formatted I/O is still verified manually/QEMU-side until we automate a userland harness.
- ✅ Heap bootstrapper (`malloc`, `free`, `aligned_alloc`) via anonymous `mmap`.
- 🚧 Buffered stdio, scanners (`scanf` family), and richer error strings remain future work.
 - 🔄 `mosh` now routes through libc for string manipulation, pipes/dup/fork/exec, and shared syscall helpers; remaining direct waitpid/poll hooks stay via the thin wrappers for now.
4. When the real cross compile build lands, update the wrapper scripts to call
   the new binaries without changing consumer workflows.

## Open Questions

- Constructor/destructor ordering: do we need `.init_array` support immediately
  or can it wait? (Initial version will stub it.)
- TLS and `errno`: single global integer suffices until threading (#109) lands.
- Testing strategy: add host-side tests for `crt0`/`exit` using the existing
  Unity suite once we can link user objects for testing, and capture `/bin/ps`
  output via a future userland harness so `SYS_PROC_LIST` stays regression-free.
