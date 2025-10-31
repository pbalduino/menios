# Tools Directory Overview

The `tools/` directory hosts thin wrapper scripts that standardize how the build
system and developers invoke compiler and binutils entrypoints for Menios. The
wrappers provide consistent defaults, enforce the correct sysroot, and respect
environment overrides so both cross and host builds stay reproducible.

## Wrapper Scripts

- `tools/menios-gcc.sh` — front-end for the C compiler and linker. It discovers
  the preferred toolchain (explicit `MENIOS_HOST_CC`, `${MENIOS_CROSS_PREFIX}-gcc`,
  or plain `gcc`) and prepends Menios-required flags:
  - `MENIOS_HOST_BUILD=1` switches the wrapper into host tool mode, allowing
    unit tests to compile with a native compiler.
  - Otherwise the script injects freestanding and architecture flags, disables
    the red zone and SSE by default, and points to the SDK sysroot populated by
    `make sdk`. `MENIOS_ENABLE_SSE=1` relaxes the floating-point restrictions.
  - `MENIOS_HOST_CFLAGS`, `MENIOS_HOST_LDFLAGS`, and `MENIOS_HOST_BUILD` let
    developers tune host builds without touching makefiles.

- `tools/menios-ar.sh` — chooses the correct archiver (`MENIOS_HOST_AR`,
  `x86_64-elf-ar`, or `ar`) and forwards all arguments unchanged.

- `tools/menios-ranlib.sh` — mirrors the archiver wrapper for `ranlib`, checking
  `MENIOS_HOST_RANLIB` first, then `x86_64-elf-ranlib`, and finally `ranlib`.

All scripts treat `MENIOS_SDK_ROOT` as the canonical sysroot. If the variable is
unset they fall back to `${repo}/build/sdk` and, for backward compatibility,
the repository root.

## Build Integration

When `make sdk` runs, the build copies these wrappers into
`build/sdk/bin/menios-*` (and the matching `x86_64-menios-*` aliases) so that
userland components can rely on a self-contained toolchain. User applications
and helper makefiles reference the staged copies—for example,
`vendor/genericdoom/Makefile.menios` exports `MENIOS_GCC=$(ROOT_DIR)/build/sdk/bin/menios-gcc`.

The same wrappers are threaded through optional host toolchain builds. During
`make toolchain`, the top-level makefile sets `AR=$(abspath tools/menios-ar.sh)`
and `RANLIB=$(abspath tools/menios-ranlib.sh)` while building binutils so that
all leaf builds share the same selection logic.

## Typical Usage

Developers normally invoke the wrappers indirectly via `make userland` or
feature-specific make targets. They can also run them by hand, e.g.:

```sh
MENIOS_HOST_BUILD=1 tools/menios-gcc.sh -o test test.c
```

When doing so, ensure `MENIOS_SDK_ROOT` points at a populated SDK (usually
`build/sdk`) if the default layout does not match the working tree.
