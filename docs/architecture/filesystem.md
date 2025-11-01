# Filesystem Library

The storage stack now includes a minimal filesystem layer capable of mounting
and reading FAT32 volumes discovered on GPT-partitioned disks. The initial
implementation is intentionally small but provides the primitives needed to load
configuration files and, eventually, user binaries from persistent storage.

## Design Overview

- **Mount points** – `fs_mount_fat32_first()` scans the attached block device
  for GPT entries, validates their boot sector, and mounts the first partition
  that exposes a FAT32 volume. Callers receive an opaque `fs_mount_t` handle
  describing the mounted filesystem.
- **GPT awareness** – The mount logic parses the primary GPT header and partition
  table directly from the disk, avoiding assumptions about hard-coded offsets.
  Only partitions with a non-zero type GUID are considered; each candidate is
  validated against the FAT32 BPB before use.
- **In-memory FAT** – The driver loads the first FAT into kernel memory during
  mount. The table provides fast cluster-chain traversal for file and directory
  operations while keeping the code simple. The current disk image requires only
  a few hundred KiB for the FAT copy.
- **Read operations** – The API focuses on safe read access:
  - `fs_list_directory()` iterates a directory tree using a callback.
  - `fs_file_read()` streams a portion of a file into a caller-provided buffer.
  - `fs_file_read_all()` returns a kernel-allocated buffer containing the full
    contents of a file. The buffer is NUL-terminated to simplify text handling.
- **Write support (incremental)** – Buffered writes now cover truncation and
  overwriting existing files, as well as creating new directory entries for
  both files and subdirectories. The writer emits the full Long File Name (LFN)
  sequence alongside a unique 8.3 short-name alias so long filenames survive
  round-trips through legacy FAT tooling.
- **LFN support** – Long File Name entries are reconstructed so callers can use
  the human-readable names present on the EFI system partition (e.g.,
  `limine.conf`, `EFI/BOOT/BOOTX64.EFI`). The driver falls back to short 8.3
  aliases when no LFN metadata exists.

## Integration Points

- `src/kernel/fs/fat32.c` implements the GPT scanner, FAT32 parser, and the
  public API surface declared in `include/kernel/fs.h`.
- `user_demo_launch()` now demonstrates the stack end-to-end: it mounts the SATA
  disk through the VFS, walks the FAT32 directory tree (depth-limited), and logs
  both the top-level entries and the first layer of children to `com1.log`.
- The FAT32 mount plugs into the VFS dispatcher declared in `include/kernel/vfs.h`.
  `vfs_mount_fat32_root()` discovers the first GPT partition, registers it at
  the `/` mountpoint, and exposes generic helpers such as `vfs_list()` and
  `vfs_open()`. The open helper now returns `int` error codes and fills an
  output `file_t*`, rejecting write/create flags (`-EROFS`) while falling back
  to a read-only, buffered view when the filesystem driver does not offer a
  native `open` implementation.
- The VFS layer feeds the existing file-descriptor subsystem (`file.c`), so
  `fopen("/path", "r")` transparently loads data from the mounted FAT32 volume
  while other sources (serial, framebuffer) continue to use bespoke handlers.
- Userspace now reaches the filesystem via the usual Unix syscalls: `open`
  installs descriptors backed by the VFS, `read`/`write` reuse the existing file
  helpers, and `lseek` walks the per-file offsets maintained by the buffered
  `vfs_open()` context.
- Regression coverage exercises the syscall dispatcher and VFS bridge end to
  end: a Unity harness issues `SYS_OPEN`/`SYS_READ`/`SYS_LSEEK`/`SYS_CLOSE`
  requests, validating descriptor allocation, stream positioning, and error
  propagation for missing files or unsupported write flags.

## File Metadata (stat family)

The filesystem layer now exposes file metadata through a unified infrastructure
spanning the kernel, VFS, and individual filesystem drivers. This enables
userspace programs to query file properties via the POSIX stat() family.

### Architecture

**Kernel Infrastructure** (`include/kernel/fs.h:24`, `src/kernel/fs/fat32.c:2721`):
- `struct fs_path_info` — uniform descriptor for file metadata including size,
  permissions, timestamps, file type, ownership, and inode number.
- `fs_path_info_to_stat()` — helper to convert `fs_path_info` to POSIX
  `struct stat` format for userspace consumption.

**VFS Integration** (`include/kernel/vfs.h:21`, `src/kernel/fs/vfs.c:203`):
- `vfs_path_info()` — query function to retrieve metadata for a given path.
  Resolves the path through the VFS namespace and delegates to the appropriate
  filesystem driver.
- `.stat` callback in `file_ops` — allows individual drivers to report metadata
  for open file descriptors. The VFS invokes this during `fstat()` syscalls.
- Metadata caching — when opening files, the VFS caches `fs_path_info` data for
  efficient repeated queries via `fstat()` without re-parsing directory entries.

**Syscall Interface** (`include/menios/syscall.h:40`,
`src/kernel/syscall/syscall.c:933`):
- `SYS_STAT` (89) — retrieve metadata by path, following symbolic links
- `SYS_LSTAT` (90) — retrieve metadata by path, without following symlinks
  (currently identical to `SYS_STAT` since symlinks are not yet implemented)
- `SYS_FSTAT` (91) — retrieve metadata for an open file descriptor

All three syscalls validate inputs, resolve paths or file descriptors through
the VFS, invoke the appropriate driver callbacks, convert results to
`struct stat` format, and copy the result to userspace memory.

**Libc Wrappers** (`src/libc/stat.c:20`, `src/libc/unistd.c:325`,
`user/libc/realpath.c:1`):
- `stat()`, `lstat()`, `fstat()` — delegate directly to the corresponding
  syscalls, handling error conversion and errno setting.
- `access()` — uses `stat()` to check real mode bits for file accessibility,
  replacing the previous stub that unconditionally failed.
- `realpath()` — canonicalizes paths and validates existence with `stat()`,
  ensuring POSIX-correct behavior.
- `pathconf()` — returns POSIX-compliant values for `_PC_PATH_MAX`, `_PC_NAME_MAX`,
  `_PC_LINK_MAX`, `_PC_CHOWN_RESTRICTED`, `_PC_NO_TRUNC` (issue #368 closed).

### Driver Support

**FAT32** (`src/kernel/fs/fat32.c`):
- Implements `.stat` callback (`fat32_stream_stat()`).
- Populates `fs_path_info` with cluster-derived metadata including timestamps,
  DOS attribute byte, and derived POSIX mode bits.
- Converts the FAT timestamps and attributes for both path-based queries and
  open-file handles so VFS caching stays coherent.
- **Remaining limitations** (tracked in issue #367 unless noted):
  - Timestamp granularity follows FAT32 rules (2s resolution for mtime, date-only for atime).
  - Long filename (LFN) metadata is not used to refine permissions or locales.
  - POSIX ownership remains synthetic (uid/gid are always zero).

**Pseudo-filesystems**:
- **tmpfs, procfs, devfs, pipes, console devices** — now expose fully populated
  `.stat` callbacks with synthetic metadata (type, permissions, timestamps,
  inode identifiers). Timestamps derive from boot-time monotonic clocks to
  provide stable, non-zero values even though the data is virtual.

### Use Cases Enabled

- **ls command** — Can display file sizes, permissions, and timestamps (once
  full metadata is implemented).
- **Build tools** — `make` and other build systems can use modification times
  for incremental builds.
- **File tests** — Shell scripts can use `test -f`, `test -d`, etc. to check
  file types.
- **find command** — Can filter by file type, size, or modification time.
- **realpath()** — Resolves canonical paths and verifies existence.
- **access()** — Checks file permissions correctly.

### Outstanding Work

- ~~**Issue #367** — Parse rich FAT32 metadata (timestamps, DOS attributes, LFN
  information).~~ ✅ Completed
- ~~**Issue #317** — Implement `utime()` syscall to modify file timestamps~~ ✅ Completed
- **Issue #368** — Complete `pathconf()` implementation for all POSIX queries.

## Limitations and Follow-up Work

- Write support remains limited to buffered, single-writer use cases; unlinking
  currently handles regular files and empty directories, but recursive pruning
  or concurrent writers still need design work.
- Only the primary GPT is consulted. Mirroring, MBR fallbacks, and partition
  attributes are not validated yet.
- The global block cache (Issue #63) keeps frequently accessed sectors resident,
  so repeated directory walks avoid redundant DMA transactions.
- There is no generic mount manager; callers track their own `fs_mount_t`
  handles. The upcoming VFS layer (Issue #65) should centralize mount tables
  and expose filesystem namespaces.

Despite these gaps, the kernel can now load configuration files directly from
its install media—an essential milestone on the road to user-space loaders and a
shell.

## Future: ext2 Filesystem Support

While FAT32 provides a functional foundation, meniOS is planning to add ext2
(Second Extended Filesystem) support to enable proper UNIX filesystem features.

### Motivation

ext2 offers significant advantages over FAT32:
- **POSIX permissions** — full rwx permission model with user/group/other
- **Ownership** — UID/GID tracking for multi-user support
- **Symbolic links** — native symlink support for flexible filesystem organization
- **Hard links** — multiple directory entries pointing to same inode
- **Better metadata** — access/modification/change times with second precision
- **Timestamps** — proper UNIX timestamps instead of DOS date/time format
- **Larger files** — support for files up to 4GB (with potential for larger)
- **Standard Linux FS** — compatible with all Linux tools and utilities

### Implementation Roadmap

See [Filesystem Infrastructure Diagram](../diagrams/issue_dependencies_filesystem.png)
for the complete dependency graph.

#### Phase 1: Read-Only Support (#154) — 2-3 weeks
- Superblock parsing and validation (magic 0xEF53)
- Block group descriptor reading
- Inode structures and metadata
- Directory traversal (ext2_dir_entry parsing)
- File reading via direct blocks (files ≤48KB)
- VFS integration for read operations

**Deliverable:** Can mount and read ext2 partitions (read-only mode)

#### Phase 2: Indirect Block Support (#227 partial) — 1 week
- Single indirect block pointers (files up to 4MB)
- Double indirect block pointers (files up to 4GB)
- Triple indirect block pointers (theoretical 4TB support)
- Large file reading capabilities

**Deliverable:** Can read large files from ext2 partitions

#### Phase 3: Write Support (#227 full) — 2-3 weeks
- Block allocation via bitmap management
- Inode allocation and freeing
- File creation and writing
- Directory creation (mkdir)
- Metadata updates (timestamps, permissions)

**Deliverable:** Full read-write ext2 support

#### Phase 4: Advanced Operations (#227 complete) — 2 weeks
- File and directory deletion
- Hard link creation (link)
- Symbolic link creation (symlink) and following
- Permission management (chmod, chown)
- File truncation and resize

**Deliverable:** Complete ext2 implementation with all standard operations

#### Phase 5: Multi-Partition Infrastructure (#420, #421) — 4-6 weeks
- **Partition table parsing** (#420):
  - GPT (GUID Partition Table) support
  - MBR (Master Boot Record) support
  - Partition type detection (FAT32, ext2/3/4, Linux swap, etc.)
- **VFS mount points** (#421):
  - Mount point data structures
  - Path lookup across mount boundaries
  - Mount/unmount operations
  - /proc/mounts support

**Deliverable:** Can detect and mount multiple partitions

#### Phase 6: Dual-Partition Boot (#228) — 5 weeks
- FAT32 boot partition mounted at `/boot`
- ext2 root partition mounted at `/`
- Kernel command line parsing (root=, rootfstype=)
- Boot sequence integration
- Disk image creation scripts (dual partition layout)
- Standard Linux filesystem hierarchy (FHS compliance)

**Deliverable:** Production boot configuration with proper FS separation

#### Phase 7: Binary Migration (#229) — 1.5-2 weeks
- Build system updates to install binaries on ext2
- Migration of `/bin` utilities from FAT32 to ext2
- Proper permission setup (755 for executables)
- Ownership configuration (root:root)
- Symbolic link creation (`/bin/sh` → `mosh`)
- Environment configuration (`/etc/environment`, PATH)

**Deliverable:** System binaries live on ext2 with proper permissions

### Timeline Summary

- **ext2 read-only:** 2-3 weeks (#154)
- **ext2 full write:** 8-11 weeks total (#227)
- **Multi-partition infra:** 4-6 weeks (#420, #421)
- **Dual-partition boot:** 5 weeks (#228)
- **Binary migration:** 1.5-2 weeks (#229)

**Total estimated:** ~23-31 weeks for complete ext2 + dual-partition setup

### Architecture After ext2

```
Disk Layout:
┌────────────────────────────────────┐
│ GPT Header                         │
├────────────────────────────────────┤
│ Partition 1: FAT32 (100MB)         │ ← /boot
│   - Bootloader (Limine)            │
│   - Kernel (kernel.elf)            │
│   - Boot config (limine.cfg)       │
│   - initrd (future)                │
├────────────────────────────────────┤
│ Partition 2: ext2 (remainder)      │ ← /
│   - /bin (system binaries)         │
│   - /etc (configuration)           │
│   - /home (user files)             │
│   - /usr (user programs)           │
│   - /var (variable data)           │
│   - /tmp (tmpfs overlay)           │
│   - /dev (devfs)                   │
│   - /proc (procfs)                 │
│   - /boot (mount point for part 1) │
└────────────────────────────────────┘
```

### Benefits

**Security:**
- Proper file permissions prevent unauthorized access
- Ownership model supports multi-user environments
- Executable bit prevents accidental code execution

**Organization:**
- Boot files isolated from system files
- Standard Linux directory hierarchy (FHS)
- Symbolic links enable flexible organization

**Compatibility:**
- Can mount Linux-formatted USB drives
- Compatible with standard Linux tools (mkfs.ext2, fsck, etc.)
- Foundation for ext3/ext4 upgrades (journaling)

**Features:**
- UNIX semantics for all file operations
- Native support for POSIX applications
- Better developer experience (familiar FS)

### Current Status

- ✅ **Foundation complete:** Block device (#62), block cache (#63), VFS (#65)
- ✅ **FAT32 working:** Full read-write support (#189)
- 📋 **ext2 planned:** Issues #227, #154, #228, #229, #420, #421 created
- 📊 **Priority:** Medium (not blocking Doom or GCC milestones)
- 🎯 **Recommendation:** Start with #420 (partitions) + #154 (ext2 read-only)

See the [Filesystem Infrastructure Diagram](../diagrams/issue_dependencies_filesystem.png)
for detailed dependency relationships and implementation phases.
