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
- **Read-only operations** – The API focuses on safe read access:
  - `fs_list_directory()` iterates a directory tree using a callback.
  - `fs_file_read()` streams a portion of a file into a caller-provided buffer.
  - `fs_file_read_all()` returns a kernel-allocated buffer containing the full
    contents of a file. The buffer is NUL-terminated to simplify text handling.
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
  `vfs_open()`.
- The VFS layer feeds the existing file-descriptor subsystem (`file.c`), so
  `fopen("/path", "r")` transparently loads data from the mounted FAT32 volume
  while other sources (serial, framebuffer) continue to use bespoke handlers.

## Limitations and Follow-up Work

- Mounting is read-only; write support, cache coherency, and FAT updates are
  out of scope for now.
- Only the primary GPT is consulted. Mirroring, MBR fallbacks, and partition
  attributes are not validated yet.
- The implementation reads entire clusters into temporary buffers. A block cache
  (Issue #63) will reduce the number of DMA transactions and enable sharing data
  between consumers.
- There is no generic mount manager; callers track their own `fs_mount_t`
  handles. The upcoming VFS layer (Issue #65) should centralize mount tables
  and expose filesystem namespaces.

Despite these gaps, the kernel can now load configuration files directly from
its install media—an essential milestone on the road to user-space loaders and a
shell.
