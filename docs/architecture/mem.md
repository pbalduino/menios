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

## Kernel Virtual Address Layout

The kernel currently relies on two key mappings:

1. **Higher-Half Direct Map**: The HHDM covers all physical memory starting at `kernel_offset` (reported by Limine). We use it for quick physical-to-virtual translations when touching page tables or zeroing newly allocated pages.
2. **Higher-Half Kernel Image**: The kernel ELF is loaded in the higher half (Limine's default). We keep Limine's layout: kernel text/data/BSS live above the canonical base (0xffffff8000000000). There is no explicit relocator; we simply reuse the bootloader's mapping.

Other points:

* Page-table helpers in `pmm.c` operate on 4-level structures (`pml4_t`, `page_directory_pointer_t`, etc.). When we need a new mapping we allocate intermediate tables on demand and set permissions (`pmm_map_page_in_root`).
* `clear_kernel_user_permissions()` walks all kernel entries and ensures the user bit is cleared so user address spaces cannot access kernel pages even if cloned.

## Heap & Kernel Allocations

* The kernel heap is initialised by `init_heap(NULL, PAGE_SIZE * HEAP_SIZE)` in `mem_init()`. At the moment the heap carves memory from the higher-half mapping; it is not yet virtual-memory aware.
* The heap feeds `kmalloc`, `kfree`, and related APIs. Memory compaction is handled by a background thread (`mem_compactor`).

## User Address Spaces (Current State)

* `proc_create_user()` clones the kernel PML4 by calling `pmm_clone_kernel_address_space()`. That routine allocates a fresh PML4, copies the kernel entries, and clears user bits for canonical high-half segments before returning the new root.
* Stacks are eagerly allocated: we compute a per-PID stack window using `user_stack_top()` and allocate `PROC_USER_STACK_SIZE` bytes worth of physical pages up-front. Each page is zeroed and mapped writable+user into the new CR3.
* The ELF loader (`elf64_load_image`) allocates new physical pages for each PT_LOAD segment, zeroes them, copies the segment bytes, and maps them into the process CR3 with the correct permissions. We record each allocation in `proc->user_segments` so `proc_exit()` can free the pages later.
* There is no separate heap segment: user processes have no `brk`/`mmap` support yet. All allocations are static segments from the ELF plus the fixed-size stack.
* Page faults are terminal. Any access outside the pre-mapped pages triggers the page-fault handler, which currently just logs diagnostics and halts.

## User Address Spaces (Planned Enhancements)

Work tracked under issue #28 will push the design forward. Key decisions so far (expanded in `docs/architecture/per_process_vm.md`):

* **Canonical Layout**: We will define fixed bases for text, rodata, data/BSS, heap, stack, and mmappable regions. PIDs will no longer influence virtual addresses.
* **Region Metadata**: Each process will own a list of regions (`vm_region_t`) describing the start, size, permissions, and behaviour (static, heap, stack, mmap). This will replace the flat `user_segments` array.
* **Lazy Allocation**: Stacks and heap will grow on demand. Only the top stack page is mapped initially; faults inside the stack window will allocate new pages and update the region metadata.
* **Page-Fault Pump**: The page-fault handler will call a new `vm_handle_page_fault()` function that consults region metadata, performs lazy allocation when allowed, or terminates the task for illegal access.
* **Tighter Kernel/User Separation**: Instead of cloning the entire kernel page table, we will build per-process PML4s that include only the required kernel shared mappings plus the user regions. That reduces attack surface and waste.
* **Cleanup**: `proc_exit()` will release regions by walking metadata, unmapping, and freeing physical pages—including lazily allocated ones.

## Open Questions / Future Work

* Demand paging for large executables or memory-mapped files is not yet designed. When we add a filesystem this document should be updated to cover file-backed mappings.
* The kernel heap still operates purely inside the HHDM; longer term we may want a VM-aware heap that can grow by mapping new virtual arenas.
* We have not finalised the layout for kernel modules, PCI MMIO windows, or per-CPU structures. When SMP work begins, the memory map will need to document per-CPU stacks and data segments.

---

_Last updated: 2025-09-26_
