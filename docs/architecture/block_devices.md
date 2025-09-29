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

## Initialization Flow

1. `block_device_system_init()` runs early during boot (invoked from `_start`
   after the heap and file layer are available).
2. Storage drivers (e.g., the AHCI PCI controller) discover hardware, obtain
   memory-mapped register ranges, and allocate bookkeeping structures.
3. Once a driver is ready, it allocates a `block_device_t`, populates metadata
   and callbacks, and registers it with the block core.
3. Higher-level subsystems—block cache, filesystem implementations, ramdisks—can
   discover available devices and issue read/write requests through the unified
   API.

## Next Steps

- The AHCI driver skeleton enumerates PCI devices with class code 0x01/0x06 and
  maps BAR5 into the HHDM, laying the groundwork for full DMA-backed transfers
  (Issue #117).
- Integrate the block layer with the upcoming block cache (Issue #63).
- Extend the API with asynchronous I/O and request queues once drivers require
  higher throughput.
