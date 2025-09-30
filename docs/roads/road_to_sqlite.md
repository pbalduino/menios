# Road to SQLite on meniOS

🎯 **Goal**: Run the stock SQLite test suite on meniOS and ship a usable `sqlite3`
command-line client for userland applications.

## Current Capabilities

- File I/O syscalls (`open/read/write/lseek/close`) and pipes are implemented.
- Processes, fork/exec, waitpid, and a PID 1 supervisor are online.
- Memory management includes `mmap/munmap`, copy-on-write, and anonymous
  allocations via `sbrk/munmap`.
- Tooling: libc stubs exist, but thread-safety and a packaging toolchain are not
  yet complete.
- Storage stack is **read-only** (FAT32) – no write-back path for databases.

## Major Prerequisites

1. **Writable Filesystem Path**
   - #61 – Filesystem write support (FAT32 write-back or ext2 port) *(BLOCKER)*
   - #145 – tmpfs/ramfs for ephemeral databases (optional but desirable)
   - #146/#147 – devfs/procfs for device discovery and monitoring (secondary)

2. **Userland Environment**
   - #152 – Environment variables (`PATH`, `TMPDIR`)
   - #151 – Working directory navigation (DONE)
   - Shell roadmap (#161-#165) to launch the SQLite CLI conveniently

3. **libc & Toolchain**
   - #109/#110 – pthread API & thread-safe libc (SQLite uses mutexes)
   - #29 – Cross-toolchain & SDK to build the SQLite sources
   - #113 – Thread-aware syscalls for observability (optional)

4. **Testing Infrastructure**
   - #135 – Code coverage / enhanced testing to catch regressions
   - TAP-style harness or port of SQLite’s TCL test runner (future)

## Implementation Phases

### Phase 0 – Foundations (In Progress)
- Complete shell prerequisites to ease userland testing (#152).
- Land filesystem write support (#61) and/or tmpfs (#145).

### Phase 1 – Writable Storage Layer (2-3 weeks)
1. Implement write path for FAT32 or bring up ext2 (#148) for better metadata.
2. Add basic locking hooks in VFS to prevent concurrent clobbering.
3. Provide `fsync`/`fdatasync` stubs or document limitations (SQLite relies on
   durable writes).

### Phase 2 – libc & Threading Support (3-4 weeks)
1. Finish pthread surface (#109) and thread-safe libc (#110).
2. Ensure `malloc` behaves under multi-threaded stress (#95).
3. Implement `fcntl` locking primitives (advisory locks) if SQLite is built with
   multi-process support.

### Phase 3 – Toolchain & Build Integration (2 weeks)
1. Package a minimalist SDK (#29) with headers, crt0, and static libc.
2. Add a `make sqlite` target that fetches SQLite sources and builds the shell
   (`sqlite3`) as a static ELF.
3. Create root filesystem staging entries so the CLI ships with the OS image.

### Phase 4 – System Integration & Testing (2-3 weeks)
1. Port/tweak SQLite’s `testfixture` harness or craft smoke tests that
   cover: create table, insert, transactions, VACUUM.
2. Measure durability semantics; document gaps (e.g., if `fsync` is a no-op).
3. Provide example apps (log viewer, settings store) using the SQLite C API.

### Phase 5 – Optional Enhancements
- Shared cache mode: requires POSIX shared memory (#104) and futexes (#105-#107).
- Write-ahead logging: depends on high-resolution timers (#101 – DONE) and
  robust fsync semantics.
- O_DIRECT/async I/O: future work once block layer supports advanced DMA.

## Dependency Summary

```
Storage Write Path (#61/#145/#148) ─┐
Thread-safe libc & pthreads (#109/#110) ─┼─> SQLite Build (#29 toolchain)
Shell & env vars (#152, #161-#165) ──────┘        ↓
                                        Build integration + tests → sqlite3 CLI
```

## Immediate Next Steps

1. Finish environment-variable support (#152) for shell + build scripts.
2. Prioritize filesystem write capability (#61) or ext2 port (#148).
3. Begin pthread/libc hardening (#109/#110) to ensure SQLite’s mutex layer works.

## Success Criteria

- `sqlite3` CLI runs on meniOS, reading and writing databases on disk or tmpfs.
- Core regression tests (minimum: `quick.test`, `select1.test`, `wal.test`) pass.
- Documentation describes durability guarantees and any unsupported pragmas.
- Toolkit includes headers/libs so third-party apps can link against SQLite.

Tracking: see also `docs/roads/road_to_doom.md` (userland focus) and
`docs/roads/road_to_shell.md` for complementary prerequisites.
