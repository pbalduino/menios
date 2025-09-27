# kmalloc & Virtual Memory Integration

Issue #6 moves the kernel heap off the raw HHDM pointer space and into a managed virtual arena. As of 2025‑09‑26 the heap now reserves a 64 MiB window at `KHEAP_BASE` and maps freshly allocated physical pages into that window before handing them to the allocator. This section documents what is in place and what remains.

## Implementation Snapshot

* `KHEAP_BASE`/`KHEAP_SIZE` define the arena. `heap_map_region()` wraps every `pmm_alloc_pages()` call, mapping pages via `pmm_map_page()` and advancing `heap_next_vaddr`.
* The existing region metadata still tracks the backing physical pages; future work will extend it to report virtual coverage and add guard pages.
* The heap continues to use spinlocks for synchronisation, so concurrency characteristics are unchanged.

## Next Steps

1. **Shrinking** – Teach `heap_release_region_if_unused()` to unmap pages and return virtual space when an entire region is freed.
2. **Diagnostics** – Expose committed vs. reserved virtual space in `heap_get_stats()` and add optional guard pages/red zones.
3. **SMP Optimisations** – Explore per-CPU arenas or magazines once SMP scheduling lands.

With the arena in place, future kmalloc work can build on standard VM machinery instead of the implicit HHDM shortcut.

_Last updated: 2025-09-26_
