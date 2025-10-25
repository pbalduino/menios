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
- `pathconf()` — partially implemented; returns `_PC_PATH_MAX` support. Other
  queries still return `-ENOSYS` (tracked in issue #368).

### Driver Support

**FAT32** (`src/kernel/fs/fat32.c`):
- Implements `.stat` callback (`fat32_stream_stat()`).
- Parses directory entries to extract file size and read-only attribute.
- **Current limitations** (tracked in issue #367):
  - Timestamps (creation, modification, access) are not yet parsed from the FAT
    directory entry date/time fields.
  - DOS attributes beyond read-only (hidden, system, archive, volume) are
    ignored.
  - Long filename (LFN) metadata is not extracted for timestamps or permissions.
  - Permissions are hard-coded (0644 for files, 0755 for directories) rather
    than derived from FAT attributes.
  - No ownership information (all files appear as uid=0, gid=0).

**Pseudo-filesystems** (tracked in issue #366):
- **tmpfs, procfs, devfs, pipes, console devices** — Currently expose
  `.stat = NULL`, causing all stat queries to return `-ENOSYS`.
- Each of these drivers needs to implement a `.stat` callback to report
  appropriate metadata (file type, size for buffered data, timestamps, device
  type for character/block devices, etc.).

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

- **Issue #366** — Implement `.stat` for pseudo-filesystems (tmpfs, procfs,
  devfs, pipes, console devices).
- **Issue #367** — Parse rich FAT32 metadata (timestamps, DOS attributes, LFN
  information).
- **Issue #365** — Implement `chmod()`/`fchmod()` syscalls to modify file
  permissions (write-side of metadata).
- **Issue #317** — Implement `utime()` syscall to modify file timestamps
  (write-side of metadata).
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
