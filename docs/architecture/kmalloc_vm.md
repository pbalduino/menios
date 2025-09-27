# kmalloc & Virtual Memory Integration Plan

Issue #6 tracks the work required to back the kernel heap with the virtual memory subsystem rather than allocating directly from physical pages.

## Current Behaviour

* The heap (see `src/kernel/mem/kmalloc.c`) manages a linked list of blocks carved from contiguous chunks allocated with `pmm_alloc_pages()`.
* Each grow request grabs whole physical pages, maps them into the higher-half direct map (HHDM), and keeps the virtual address returned by `physical_to_virtual()`.
* The heap records physical regions manually via `heap_register_region()`, but the kernel has no ability to release those pages lazily or remap them elsewhere.
* There is no integration with per-process or kernel-wide virtual memory metadata; kmalloc assumes the HHDM is always available and identity-mapped with an offset.

## Target Architecture

1. **Virtual Address Arena**
   * Reserve a dedicated virtual range for the kernel heap (e.g., a high-half window that is not part of the HHDM).
   * Maintain an internal cursor/metadata describing which pages in this range are committed.

2. **Page Fault Growth**
   * On heap growth, allocate virtual space first, then request backing physical pages via `pmm_alloc_pages()` as needed, mapping them into the heap arena with `pmm_map_page()` (writable, supervisor-only).
   * Optionally leave holes unmapped initially and populate them lazily on fault, similar to user stack growth.

3. **Region Metadata Integration**
   * Register heap mappings with a kernel VM region list (similar to the userland `vm_region_t`) so diagnostics/debugging tools can report heap usage.
   * Expose APIs for querying committed vs. reserved heap space.

4. **Cleanup & Shrinking**
   * Allow returning unused spans of virtual space back to the arena and releasing physical pages via `pmm_free_pages()`.
   * Ensure that kmalloc metadata updates remain atomic with the new mapping/unmapping operations (spinlocks already guard the heap).

## Implementation Steps

1. Introduce `kmalloc_arena_init()` to reserve and track the virtual window.
2. Replace direct `pmm_alloc_pages()` calls in `heap_grow()` with a helper that maps pages into the arena and returns the virtual base.
3. Update `heap_region` bookkeeping to record virtual base/size in addition to physical pages for easier debugging.
4. Audit compactor/shrink paths to unmap and free physical pages when possible.
5. Extend diagnostics (`heap_get_stats`) to report committed vs. reserved virtual space.

Once these steps are in place we can flip issue #6 to Done and move toward advanced features like guarded red zones and per-CPU heaps.

_Last updated: 2025-09-26_
