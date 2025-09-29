# Block Device Architecture

The kernel now exposes a lightweight block device abstraction that allows
storage drivers to register themselves and provide uniform read/write services
to higher layers.

## Core Concepts

- **`block_device_t`** encapsulates a logical device: name, block size, total
  block count, driver context, and a set of callbacks.
- **Operations** – Drivers implement the `block_device_ops_t` tables:
  - `read_blocks()` performs synchronous reads.
  - `write_blocks()` (optional) handles writes.
  - `flush()` (optional) persists outstanding buffered writes.
- **Registry** – The block layer maintains a global list of registered devices,
  guarded by a mutex. Drivers call `block_device_register()` when ready; lookup
  APIs (`block_device_lookup`, `block_device_first/next`) provide discovery.
- **Synchronous helpers** – Kernel clients can use
  `block_device_read()`/`block_device_write()` and `block_device_flush()` to
  issue simple operations without worrying about the driver details.
- **Block cache** – `block_cache.c` tracks a small LRU of recently accessed
  sectors (currently 256 entries) and performs write-through updates. Reads hit
  the cache first and fall back to the driver on a miss; successful reads or
  writes refresh the cached copy. Devices with sector sizes larger than 4 KiB
  bypass the cache automatically.

## Initialization Flow

1. `block_device_system_init()` runs early during boot (invoked from `_start`
   after the heap and file layer are available).
2. Storage drivers (e.g., the AHCI PCI controller) discover hardware, obtain
   memory-mapped register ranges, and allocate bookkeeping structures.
3. Once a driver is ready, it allocates a `block_device_t`, populates metadata
  and callbacks, and registers it with the block core.
4. The AHCI driver provisions one DMA command slot per active SATA port, keeps a
   pre-allocated bounce buffer for 512 KiB transfers, submits READ/WRITE DMA
   commands, and acknowledges completions through the shared IRQ path.
5. Higher-level subsystems—block cache, filesystem implementations, ramdisks—can
   discover available devices and issue read/write requests through the unified
   API.

## Next Steps

- SATA disks are now exposed as `sataN` block devices backed by DMA reads and
  writes, with interrupts unmasking completion on vector `0x40`. During boot the
  VFS mounts the FAT32 volume at `/`, and `user_demo_launch()` logs a
  depth-limited directory tree to the serial console as a smoke test.
- Block cache already reduces redundant reads for FAT32 and future filesystems;
  once write-back policies are needed we can extend the current write-through
  implementation.
- Extend the API with asynchronous I/O and request queues once drivers require
  higher throughput.
