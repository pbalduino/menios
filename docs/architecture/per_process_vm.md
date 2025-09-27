# Per-Process Virtual Memory Plan

This document captures the current state of meniOS user address spaces and the plan that drove issue #28 (per-process virtual memory management). The initial implementation now in trunk covers region metadata, lazy-growing stacks, and user-mode page-fault recovery. Remaining enhancements (heap growth, demand paging) build on this foundation.

## Current Behaviour

* The kernel assigns a new `proc_info_t` structure for each process and clones the kernel PML4 via `pmm_clone_kernel_address_space()`.
* Stacks are allocated eagerly: `PROC_USER_STACK_SIZE` bytes are backed by contiguous physical pages and identity zeroed before being mapped. Stack virtual addresses are derived from helper functions `user_stack_top()` / `user_code_base()` using a simple PID-based stride.
* Executable segments are loaded by `elf64_load_image()`, which allocates fresh physical pages for each PT_LOAD chunk, maps them writable/executable as needed, and records the physical ranges in `proc->user_segments` so they can be freed on exit.
* There is no metadata wider than the raw segment list: the kernel does not track regions (code, data, heap, guard pages) beyond the physical allocations returned by the loader. Process teardown frees whatever was registered, assuming eager allocation covered everything.
* Page faults are treated as fatal. There is no lazy allocation, guard page handling, or demand paging logic.

These design choices work for the tiny `user_demo` binary but do not scale to larger programs. Every process inherits the full kernel address space, every stack is fully committed, and there is no room for heap growth or large mappings without revisiting the layout.

## Target Architecture

To close #28 we need to evolve the VM infrastructure along five axes:

1. **Address-Space Layout**
   * Define a canonical user layout (text + rodata, data + bss, heap, stack, mmap gap) in a single header (`include/kernel/vm_layout.h`).
   * Replace the PID-based helpers with layout-driven base/limit helpers so PIDs do not influence virtual addresses.
   * Reserve guard pages around stacks and between heap / mmap regions.

2. **Region Metadata**
   * Introduce a per-process region table (e.g. `vm_region_t`) with permissions, virtual range, and type, stored in `proc_info_t`.
   * Refactor `elf64_load_image()` to populate regions for each PT_LOAD mapping (code, rodata, data) and a dedicated entry for the initial heap break.

3. **Lazy Allocation**
   * Only map the top stack page and mark the surrounding guard pages as unmapped.
   * Seed the heap region without mapping; keep the initial `brk` at the heap base and grow via lazy page faults.
   * Update `proc_register_user_segment` usage so physical allocations are recorded per region instead of a flat array.

4. **Page Fault Handling**
   * Extend the page fault handler to detect faults in user regions and allocate a page on demand (`pmm_alloc_pages(1)`, zero fill, map with user permissions) when the faulting address falls inside a growable region (stack or heap).
   * Deliver a segmentation fault to the process (or kill it) if the address lies outside any registered region.

5. **Isolation & Cleanup**
   * Stop cloning the whole kernel mapping: create a minimal user PML4 that maps only kernel shared areas (higher-half identity) plus user regions.
   * Ensure `proc_exit` iterates region metadata, unmaps virtual ranges, and frees associated physical pages (including lazily allocated ones).

## Immediate Action Items

1. Draft `vm_layout.h` with constants for text, heap, stack, and mmap regions.
2. Add a `vm_region_t` list to `proc_info_t` and helper APIs (`vm_region_add`, `vm_region_find`).
3. Refactor `proc_create_user` and `elf64_load_image` to register regions instead of flat physical segments.
4. Modify the page fault handler (`src/kernel/idt.c` / `idt_pf_isr_handler`) to invoke a new `vm_handle_page_fault()` helper.
5. Update `proc_exit` to release all regions and unmapped pages.
6. Extend tests (or add new ones) to induce stack growth and heap allocation faults to validate lazy mapping.

Once these steps land we can flip issue #28 to “done” and move on to userland heaps and demand paging.
