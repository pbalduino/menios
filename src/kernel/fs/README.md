# meniOS Kernel Filesystems

The filesystem stack is split by responsibility:

- `core/` – shared helpers (e.g., pipes) and VFS wiring.
- `vfs/` – the virtual filesystem layer and adapters into mounted filesystems.
- `devfs/` – device filesystem nodes (`/dev/*`).
- `procfs/` – process information under `/proc`.
- `tmpfs/` – in-memory temporary filesystem implementation.
- `fat32/` – FAT32 block filesystem support and user adapters.

Headers follow the same layout under `include/kernel/fs/`, so new code should
add interfaces alongside the implementation directory.
