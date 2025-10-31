# Memory Architecture Notes

This note collects the decisions we have made around memory management in meniOS and the current state of the implementation. The focus is to document the boot-time virtual address layout, the physical allocator, and how user processes consume address space today (and what is planned next).

## Boot Environment

* meniOS boots via Limine. We request the following services and use their responses during memory bring-up:
  * **Memory map** (`LIMINE_MEMMAP_REQUEST`) to enumerate usable, reserved, framebuffer, and other regions.
  * **Higher-Half Direct Map (HHDM)** (`LIMINE_HHDM_REQUEST`) so the kernel can directly address all physical memory using a single offset (`kernel_offset`).
* The bootloader leaves the kernel running in long mode with a 64-bit page table already active. We keep that mapping, but normalise our own bookkeeping via `init_cr3()`.

## Physical Memory Manager

* The PMM is a bitmap allocator defined in `src/kernel/mem/pmm.c`.
* Page size is 4 KiB; the bitmap is sized by `PAGE_BITMAP_SIZE` (see `include/kernel/pmm.h`). Each bit tracks the availability of one physical page.
* During `pmm_init()` we:
  1. Initialise the bitmap marking everything used (`init_page_bitmap`).
  2. Iterate Limine's memory map and mark usable ranges as free (`bulk_page_bitmap_as_free`).
  3. Capture the HHDM offset so we can translate physical↔virtual addresses via `physical_to_virtual()` / `virtual_to_physical()`.
  4. Record the kernel's current CR3 so we know the original PML4 and can clone it for new address spaces.
* Allocation helpers (`pmm_alloc_pages`, `pmm_free_pages`) operate on contiguous runs of pages. There is no buddy allocator yet; the bitmap search is linear.
* DMA-aware helpers (`pmm_alloc_aligned_pages`) can reserve contiguous spans under a
  caller-specified physical ceiling and alignment, used by `dma_buffer_alloc`
  to provide device-friendly buffers.

## Kernel Virtual Address Layout

The kernel currently relies on two key mappings:

1. **Higher-Half Direct Map**: The HHDM covers all physical memory starting at `kernel_offset` (reported by Limine). We use it for quick physical-to-virtual translations when touching page tables or zeroing newly allocated pages.
2. **Higher-Half Kernel Image**: The kernel ELF is loaded in the higher half (Limine's default). We keep Limine's layout: kernel text/data/BSS live above the canonical base (0xffffff8000000000). There is no explicit relocator; we simply reuse the bootloader's mapping.

Other points:

* Page-table helpers in `pmm.c` operate on 4-level structures (`pml4_t`, `page_directory_pointer_t`, etc.). When we need a new mapping we allocate intermediate tables on demand and set permissions (`pmm_map_page_in_root`).
* `clear_kernel_user_permissions()` walks all kernel entries and ensures the user bit is cleared so user address spaces cannot access kernel pages even if cloned.

## Heap & Kernel Allocations

* The kernel heap is initialised by `heap_init(NULL, PAGE_SIZE * HEAP_SIZE)` in `memory_initialize()`. At the moment the heap carves memory from the higher-half mapping; it is not yet virtual-memory aware.
* The heap feeds `kmalloc`, `kfree`, and related APIs. Memory compaction is handled by a background thread (`mem_compactor`).

## User Address Spaces (Current State)

* `proc_create_user()` still clones the kernel PML4 via `pmm_clone_kernel_address_space()`, giving each process the shared higher-half mappings while providing a unique CR3 for user pages.
* Each process carries `vm_regions[]` metadata. The stack region is registered as grow-down; only the top page is mapped eagerly so the task can start executing, and further growth is handled lazily.
* `elf64_load_image()` maps PT_LOAD segments and records them in both `user_segments` (for physical cleanup) and the owning region metadata so we know exactly which virtual ranges are populated.
* There is not yet a heap region (`brk`/`mmap`), but the infrastructure for grow-up regions is ready—only the stack uses it today.
* User-mode page faults are intercepted by `vm_region_handle_page_fault()`. Faults inside growable regions allocate zeroed pages on demand; illegal or permission-violating faults still fall back to the diagnostic handler.

## User Address Spaces (Planned Enhancements)

Work remaining after issue #28 (also expanded in `docs/architecture/per_process_vm.md`):

* **Canonical Layout**: Define fixed bases for text, rodata, data/BSS, heap, stack, and mmappable regions so PIDs no longer influence addresses and we can reserve guard pages.
* **Heap Region**: Introduce a grow-up region for `brk`/`mmap`, wire it into the lazy allocation path, and converge cleanup logic on the region metadata instead of the flat `user_segments` array.
* **Address-Space Isolation**: Replace the “clone kernel CR3” strategy with a curated template that maps only the required shared kernel ranges plus user regions, then teach `proc_exit` to unmap via region descriptors.
* **Demand Paging**: Build on the region infrastructure to lazily populate PT_LOAD segments or file-backed mappings once the filesystem and VFS layers arrive.

## Open Questions / Future Work

* Demand paging for large executables or memory-mapped files is not yet designed. When we add a filesystem this document should be updated to cover file-backed mappings.
* The kernel heap still operates purely inside the HHDM; longer term we may want a VM-aware heap that can grow by mapping new virtual arenas.
* `/proc/meminfo` (procfs) now reports physical allocator statistics (usable/free bytes) so userland tools can observe system memory without digging through serial logs. For user-mode heap details, libc exposes `menios_malloc_stats()` with arena/direct-mmap counters.

* We have not finalised the layout for kernel modules, PCI MMIO windows, or per-CPU structures. When SMP work begins, the memory map will need to document per-CPU stacks and data segments.

---

_Last updated: 2025-09-26_
