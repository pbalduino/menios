# Per-Process Virtual Memory Plan

This document captures the current state of meniOS user address spaces and the plan that drove issue #28 (per-process virtual memory management). The implementation now in trunk covers region metadata, eager PT_LOAD registration, lazy-growing stacks, and user-mode page-fault recovery. Remaining enhancements (heap growth, demand paging, bespoke user CR3 layouts) build on this foundation.

## Implementation Snapshot

* Every process carries a `vm_regions[]` table (`vm_region_t`) that records the virtual range, permissions, and growth behaviour for code, rodata, data, stack, and future heap/mmap regions.
* `proc_create_user()` registers the stack as a grow-down region. Only the top page is mapped initially; the rest of the stack window is populated on demand via page faults.
* `elf64_load_image()` classifies each PT_LOAD segment by its flag bits, maps the pages, and records the range in the owning region metadata so teardown can free it later.
* The page-fault handler (`vm_region_handle_page_fault`) intercepts user faults and allocates a fresh page when the address falls inside a grow-down (stack) or grow-up region. Unexpected or permission-violating accesses still trigger the traditional diagnostic path.
* Physical allocations are still tracked in `proc->user_segments` for cleanup, but new mappings are also reflected in the region metadata (`committed_base`/`committed_top`).

## Roadmap (Post-#28)

1. **Canonical Layout**
   * Introduce a shared `vm_layout.h` describing text, rodata, data, heap, stack, and mmap windows (today stack starts via `user_stack_top()`; code still relies on PID-strided helpers).
   * Reserve guard pages between regions once the new layout is in place.

2. **Heap Support**
   * Carve out a grow-up region for the process heap (`brk`/`sbrk`) and hook it into the lazy page allocator.
   * Replace the flat `user_segments` bookkeeping with region-aware structures so teardown can free lazily allocated heap pages.

3. **Address Space Isolation**
   * Replace the “clone kernel CR3” approach with a curated template that maps only shared kernel ranges plus user regions.
   * Harden `proc_exit` to iterate regions, unmap virtual ranges, and release physical pages without relying solely on `user_segments`.

4. **Testing & Tooling**
   * Add self-tests that force stack expansion and trigger fault recovery paths.
   * Once the heap is wired, stress the allocator by forcing repeated grow/shrink cycles.

With these follow-ups in place we can move toward demand paging and file-backed mappings while keeping the region infrastructure as the central source of truth.

## VM Manager API (Issue #57)

With the introduction of `vm_map`, `vm_unmap`, and `vm_clone`, user address-space management now has a kernel-facing API:

* `vm_map(proc, params)` reserves a region, allocates physical pages, zeroes them, maps them into the target CR3, and updates both region metadata and the legacy `user_segments[]` bookkeeping.
* `vm_unmap(proc, base, length)` removes page table entries and frees the backing frames for the specified range, then drops the region descriptor. (Current implementation assumes whole-region unmap; partial unmap support is a follow-up.)
* `vm_clone(child, parent)` duplicates the parent’s region table, allocates fresh frames for each committed page, copies contents, and maps them into the child.

Limitations:
* No copy-on-write yet; clone eagerly copies committed pages.
* `vm_unmap` currently frees entire regions at once—page-granular tear-down will come alongside mmap/heap work.
* Guard pages and canonical per-process layouts are still pending (see roadmap above).

These APIs bridge the earlier region metadata work with actual page-table manipulation, enabling higher-level features (heap grow, mmap, fork/exec) to advance.

## Process Creation (Issue #93)

With vm_clone in place, meniOS now offers a full `fork`/`execve` path:

* `proc_fork()` allocates a child `proc_info_t`, clones the kernel PML4, and calls `vm_clone()` to duplicate committed user regions into the new address space. The syscall trampoline copies the parent frame into the child so both return to user space with distinct return values (`0` in the child, `child_pid` in the parent).
* `proc_exec_image()` stages the replacement image in a fresh CR3, mapping a clean stack region before running the ELF loader. On success the old user mappings are torn down, the new root installs in the process, and the syscall frame is reset with pristine registers and user segments so the caller resumes in ring 3 at the ELF entry point.
* `SYS_fork` and `SYS_execve` dispatch into the helpers above, performing basic pointer/size validation and propagating negative errno values back to user mode on failure.
* The user demo program now exercises `fork`, emitting per-branch messages before the child exits, keeping the example deterministic while we wire up richer userland payloads.

This closes the loop on process cloning and image replacement: the scheduler can now spin up arbitrary user tasks, duplicate them, and hand control over to new executables without rebooting the kernel.
