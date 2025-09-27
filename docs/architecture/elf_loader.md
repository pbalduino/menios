# Userland ELF Loader Overview

This document summarises the current ELF64 loading path in meniOS and the pieces that fulfil issue #03.

## High-Level Flow

1. **Build step**: `src/usermode/user_demo.S` is assembled and linked into `obj/usermode/user_demo.elf` with `linker/user_elf.ld`.
2. **Embedding**: `objcopy` converts the ELF binary into an object file (`obj/usermode/user_demo_elf.o`) that exports three symbols: `user_demo_elf_start`, `user_demo_elf_end`, and `user_demo_elf_size`.
3. **Launch**: `src/kernel/user/user_demo.c` allocates a new process, passes the embedded byte range to `proc_create_user`, and schedules it.
4. **Load**: `proc_create_user` calls `elf64_load_image`, which parses and maps the ELF into the process address space.
5. **Execution**: After loading, the new task starts at the ELF entry point (observed in the serial log when `user_demo` prints via `write(1, …)` and exits with syscall 60).

## Loader Responsibilities

The loader lives in `src/kernel/user/elf_loader.c` and provides `elf64_load_image(proc, root_phys, image_ptr, size, entry_out)`.

Key behaviour:

- **Validation**: Checks for ELF magic (`0x7f 'ELF'`), 64-bit little-endian class, static executable type, and presence of a program header table. It also verifies that the table fits inside the supplied blob.
- **Segment iteration**: Walks every program header, ignores non-loadable entries, and handles PT_LOAD segments by:
  * Allocating enough physical pages to cover the in-memory extent (`p_memsz`).
  * Zeroing the allocation, then copying the file portion (`p_filesz`).
  * Mapping each page into the new process page tables with the proper writable flag derived from `p_flags`.
  * Recording the physical pages via `proc_register_user_segment` so `proc_exit` can free them later.
- **Entry point**: Writes the `e_entry` value to `entry_out` (used to initialise `rip`).
- **Permissions**: Uses `pmm_map_page_in_root` with the user bit set, ensuring Ring 3 code cannot access kernel-only regions.

## Error Handling

The loader is strict: any malformed header, missing program table, or mapping failure prints an explanatory message to the serial console and aborts the boot via `halt()`. This deterministic behaviour keeps the early bring-up phase simple; once the virtual-memory refactor (#28) lands we can convert some of these to process-local failures.

## Testing & Verification

- Booting `make build run` should show `Hello from user ELF via int 0x80!` on both the serial log (`com1.log`) and the screen, demonstrating that:
  * The ELF was parsed and mapped.
  * The syscall path works (`write` followed by `exit`).
- For deeper inspection you can enable serial loader logs or use `objdump -x build/obj/usermode/user_demo.elf` to inspect the segments that the loader will map.

## Future Extensions

- Multiple user binaries: the loader already supports arbitrary ELF images; once we have a filesystem it can be wired to load files from disk.
- Demand paging: with per-process VM metadata (#28) we can lazily map PT_LOAD segments rather than copying them eagerly.
- Relocation support: the current design assumes fully static executables. Dynamic relocations would require additional processing before mapping.

_Last updated: 2025-09-26_
