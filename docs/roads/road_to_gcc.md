# Road to a GCC Toolchain for meniOS

🎯 **Goal**: Ship an officially supported cross-compilation toolchain (binutils +
GCC + libc headers) so developers can build userland applications for meniOS
without relying on ad-hoc scripts.

## Current Capabilities

- Kernel builds natively on the host with GCC but exposes only minimal userland
  headers and libc stubs.
- ELF loader, process management, and syscall ABI are stable enough for static
  binaries.
- Environment variables and working directories are live (#151/#152 ✅), easing
  build-script integration.

## Key Dependencies

1. **Toolchain Infrastructure**
   - #29 – Cross-compiler & SDK packaging *(BLOCKER)*
   - #109/#110 – pthread API & thread-safe libc (needed for full libc surface)
2. **Filesystem Support**
   - Writable path for staging toolchain files (#61) or tmpfs (#145)
3. **Shell / CLI Experience**
   - Shell core (#161-#165) to run build scripts and host-side utilities
4. **Testing & QA**
   - #135 – Code coverage & automated tests for libc/syscall ABI

## Implementation Phases

### Phase 0 – Preparation (1-2 weeks)
- Finalize libc headers for exposed syscalls (unistd, fcntl, signal, env).
- Document ABI (calling convention, syscall numbers, struct layout).
- Audit current libc stubs to ensure they match POSIX expectations.

### Phase 1 – Binutils Bootstrap (2-3 weeks)
1. Build a cross `binutils` targeting `x86_64-menios`.
2. Provide wrapper scripts (`menios-as`, `menios-ld`).
3. Store artifacts under `toolchain/bin/` and add Makefile helpers to fetch or
   build them reproducibly.

### Phase 2 – GCC Stage 1 (3-4 weeks)
1. Configure GCC for bare-metal cross compilation (no libc).
2. Supply `crt0`, startup code, and minimal libc stubs (already present but may
   need polishing).
3. Enable `-nostdlib` builds for simple hello-world userland tests.

### Phase 3 – GCC Stage 2 with libc (4-6 weeks)
1. Harden libc (reentrant stdio, malloc, pthread stubs #109/#110).
2. Provide `<pthread.h>`, `<signal.h>`, `<stdlib.h>` surfaces required by GCC
   runtime.
3. Rebuild GCC with libc awareness and ship standard runtime objects
   (`libgcc`, `libstdc++` optional).

### Phase 4 – Packaging & Distribution (2 weeks)
1. Create a reproducible build script (`make toolchain` or dedicated Python
   helper).
2. Upload toolchain tarballs/checksums; integrate with CI if available.
3. Document installation and update instructions for developers.

### Phase 5 – Testing & Maintenance (ongoing)
- Regression suite compiling sample apps (hello world, pipes, sqlite).
- Track upstream GCC/binutils security updates.
- Maintain version matrix for Doom/SQLite roadmaps.

## Dependency Map

```
Libc Hardening (#109/#110) ──┐
Writable FS/TMP (#61/#145) ──┼─> Toolchain Packaging (#29)
Shell (#161-#165) ───────────┘       ↓
                        Binutils + GCC stages → Published SDK
```

## Immediate Next Steps

1. Close remaining libc gaps (thread safety, stdio) driven by #109/#110.
2. Prototype a binutils build using current headers to flush out ABI issues.
3. Draft documentation structure for the future SDK (`docs/toolchain/`?).

## Success Criteria

- `menios-gcc` cross toolchain builds static ELF binaries runnable on meniOS.
- libc headers and startup objects bundled for third-party developers.
- Automated job verifies the toolchain by compiling and running sample apps.
- Roadmaps (Doom, SQLite, shell) reference the ready toolchain as an available
  dependency.

See also:
- `docs/roads/road_to_shell.md` – shell tooling for developer workflows.
- `docs/roads/road_to_sqlite.md` – upcoming userland database support leveraging
  the toolchain.
