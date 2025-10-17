# VFS Streaming I/O and Buffer Cache

## Goals

- Replace the current "slurp entire file" behaviour with on-demand streaming reads and writes.
- Provide a reusable block cache (bread/bwrite) with LRU eviction and write-back support.
- Refactor FAT32 to use the block cache directly, enabling efficient block-level access.
- Add read-ahead and write-behind heuristics to hide device latency for sequential workloads.
- Preserve existing VFS semantics for callers (`vfs_open`, `read`, `write`, `close`, `lseek`).

## Current State

- `vfs_open` buffers whole files in memory (`read_all`/`write_all`).
- VFS writes flush on `close`, making large files expensive.
- FAT32 driver performs raw cluster reads/writes without caching.
- A simple block cache exists but is not used by the filesystem path.
- No read-ahead / write-behind infrastructure.

## Phases

### 1. Block Cache Infrastructure (#295)

- Introduce canonical `bread()`/`bwrite()` APIs wrapping the existing cache implementation.
- Support dirty buffers, write-back on eviction, synchronous flush paths.
- Provide statistics hooks for testing/diagnostics.

### 2. FAT32 Refactor (#296)

- Route FAT32 cluster access through the block cache.
- Replace ad-hoc `kmalloc` buffers with cached sector views.
- Ensure cluster allocation/freeing invalidates cache entries and writes dirty buffers back.

### 3. VFS Streaming I/O (#297)

- Extend VFS to support streaming handles without preloading entire files.
- Define driver hooks: `read`, `write`, `stat`, `truncate`, `create`.
- Maintain backward-compatible buffered mode for devices that lack streaming.
- Update FAT32 driver to expose streaming hooks.

### 4. Read-Ahead and Write-Behind (#298)

- Detect sequential access patterns and prefetch upcoming blocks via the buffer cache.
- Mark writes dirty and defer disk I/O until eviction/flush, with a dirty-limit heuristic to throttle write-back.
- Tunables: global dirty-buffer threshold (`BCACHE_DIRTY_LIMIT`) and a fixed readahead window.

## Testing Strategy

- Unit tests for block cache eviction, dirty buffer handling, and stats.
- FAT32 regression tests covering create/overwrite with streaming paths.
- VFS tests asserting partial reads, seek behaviour, append/truncate semantics.
- Performance smoke tests (e.g., sequential read/write of large files under QEMU).

## Risks / Open Questions

- Interaction between streaming writes and existing buffered mode (e.g., tmpfs, devfs).
- Recoverability: ensuring dirty cache buffers are flushed before shutdown.
- Memory footprint: bounding cache size and streaming buffers for large workloads.
