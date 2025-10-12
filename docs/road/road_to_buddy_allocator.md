# Road to Buddy Allocator

This roadmap documents the migration of meniOS userland memory allocator from a first-fit freelist design to a buddy allocator system.

> **Milestone Status:** The Buddy Allocator milestone is a prerequisite for both GCC and Doom milestones, as complex userland applications require a more robust and efficient memory management foundation.

## Why Buddy Allocator?

The current first-fit allocator works but has limitations:
- **External fragmentation**: Arbitrary-sized blocks can create unusable gaps
- **Coalescing complexity**: Must scan neighbors to merge free blocks
- **Performance**: Linear search through freelist for suitable blocks
- **No power-of-2 optimization**: Can't leverage alignment tricks

Buddy allocator advantages:
- **Bounded fragmentation**: Power-of-2 sizes limit worst-case fragmentation
- **Fast coalescing**: Buddy address calculated via XOR, no search needed
- **Order-segregated lists**: O(1) lookup for appropriate size class
- **Alignment guarantees**: Blocks naturally aligned to their size
- **Proven design**: Used in Linux kernel, FreeBSD, and many production systems

## Migration Plan Overview

The migration follows a careful sequence to minimize risk:

1. **Survey** → Understand current implementation
2. **Design** → Define buddy parameters and data structures
3. **Infrastructure** → Rewrite arena setup
4. **Core Logic** → Implement split/coalesce
5. **Integration** → Connect to malloc/free API
6. **Extensions** → Handle realloc and large allocations
7. **Validation** → Comprehensive testing
8. **Cleanup** → Remove old code and document

## Milestone Tracker

| Issue | Title | Status | Priority | Effort |
| --- | --- | --- | --- | --- |
| #245 | Survey Current Heap Implementation | ✅ Complete | High | 2-3 days |
| #246 | Define Buddy Allocator Orders and Configuration | ✅ Complete | High | 1-2 days |
| #247 | Rewrite Arena Setup for Buddy Allocator | ✅ Complete | High | 3-4 days |
| #248 | Implement Buddy Split and Coalesce Operations | ✅ Complete | Critical | 4-5 days |
| #249 | Integrate Buddy Allocator with malloc/free | ✅ Complete | Critical | 3-4 days |
| #250 | Adapt realloc/reallocarray for Buddy Allocator | ✅ Complete | High | 2-3 days |
| #251 | Update Direct mmap Path for Large Allocations | ✅ Complete | Medium | 2 days |
| #252 | Add Buddy Allocator Diagnostics and Tests | 🔄 Open | High | 3-4 days |
| #253 | Cleanup and Document Buddy Allocator Migration | 🔄 Open | Medium | 2-3 days |

**Total Estimated Effort:** 22-30 days

## Dependency Graph

```
#245 (Survey)
  ↓
#246 (Define Orders)
  ↓
#247 (Arena Setup)
  ↓
#248 (Split/Coalesce) ──→ #252 (Tests)
  ↓                          ↓
#249 (malloc/free) ────────→ #252 (Tests)
  ↓                          ↓
#250 (realloc)              #253 (Cleanup)
#251 (Direct mmap)
```

## Technical Deep Dive

### Buddy Allocator Fundamentals

A buddy allocator manages memory in power-of-2 sized blocks organized by "order":
- **Order k** → Block size = 2^k bytes
- Each order maintains a freelist of available blocks
- Allocation: Find smallest order that fits, split larger blocks if needed
- Deallocation: Return block to freelist, coalesce with buddy if free

### Key Algorithm: Finding Buddies

For a block at address `addr` with size `2^k`:
```c
buddy_addr = addr ^ (1 << k)
```

This XOR trick works because:
- Blocks at order k are aligned to 2^k boundaries
- Buddy pairs differ only in bit k
- XOR flips bit k to find the partner

Example (order 12 = 4096 bytes):
```
Block A: 0x100000  (bit 12 = 0)
Block B: 0x101000  (bit 12 = 1)  ← buddy
```

### Split Operation

When no block available at order k:
1. Find block at order k+1
2. Split into two buddies at order k
3. Return one buddy, add other to freelist[k]
4. Recurse if needed

### Coalesce Operation

When freeing block at order k:
1. Calculate buddy address
2. Check if buddy is free at same order
3. If yes: merge into order k+1, recurse
4. If no: add block to freelist[k]

## Configuration Decisions

### Recommended Parameters

```c
#define MIN_ORDER    5     // 32 bytes (fits header + small data)
#define MAX_ORDER    22    // 4 MiB (reasonable arena size)
#define ARENA_SIZE   (1 << MAX_ORDER)  // 4 MiB
#define NUM_ORDERS   (MAX_ORDER - MIN_ORDER + 1)  // 18 freelists
```

### Block Metadata

```c
typedef struct block_header block_header_t;

/* block_header_t carries both payload metadata and the buddy freelist hooks */
```

### Arena Structure

```c
typedef struct arena {
    void *base;                           // Start address
    size_t size;                          // Total arena size
    block_header_t *freelists[NUM_ORDERS]; // Per-order freelists
    struct arena *next;                   // Arena chain
} arena_t;
```

## Implementation Phases

### Phase 1: Survey (#245)
**Goal:** Complete understanding of current allocator

**Deliverables:**
- `grow_heap` currently mmaps the next arena (starting at 1 MiB, doubling up to 32 MiB) and seeds a single `block_header_t` that spans the arena. The block is pushed onto the global freelist and linked into `arena_list_head`.
- `block_header_t` is 0x50 bytes, aligned to 16, and stores neighbour pointers (`next/prev`), freelist linkage (`free_next/free_prev`), the owning arena pointer (NULL for direct `mmap`), the mmap base/size for direct mappings, payload size in bytes, and flags (`BLOCK_FLAG_FREE`, `BLOCK_FLAG_DIRECT`). Payload begins immediately after the header.
- Allocation is pure first-fit: `find_suitable_block` linearly walks `free_list_head`, `split_block` carves the tail of oversized blocks, and `malloc` does not segregate by size. Freeing coalesces with adjacent free blocks inside the same arena and pushes the merged block back to the freelist. Requests above `ARENA_MAX_SIZE - arena_overhead` or with large alignments fall back to direct `mmap` and bypass arena bookkeeping.
- Risk assessment: arbitrary splitting/coalescing coupled with a single freelist makes size accounting fragile (pattern mismatches observed in `/bin/malloc_stress`). Fragmentation grows quickly under heavy workloads, alignment relies on header maths, and large allocations bypass arenas entirely—motivating the switch to a deterministic buddy scheme.

### Phase 2: Design (#246)
**Goal:** Define buddy allocator parameters

**Deliverables:**
- **Order bounds.** Adopt `MIN_ORDER = 7` (128 B blocks) and `MAX_ORDER = 27` (128 MiB blocks). `MIN_ORDER` safely covers the 0x50-byte header plus 16-byte alignment; `MAX_ORDER` matches the new 128 MiB arena size. This yields 21 freelists (orders 7–27).
- **Arena size.** Each arena will map `1u << MAX_ORDER` bytes (128 MiB), aligned to 2 MiB for paging. New arenas are seeded as a single order-27 block before splitting.
- **Buddy metadata.**
  ```c
  typedef struct block_header {
      struct block_header* buddy_next;
      struct block_header* buddy_prev;
      struct arena* arena;      // Owning arena; NULL for direct mmaps
      uint32_t buddy_order;     // 7..27 inclusive
      uint32_t buddy_flags;     // BUDDY_FREE, BUDDY_USED
      uintptr_t buddy_offset;   // Offset from arena->buddy_base
      /* ... existing payload fields (mapping_base, size, flags, etc.) ... */
  } block_header_t;
  ```
  The existing 0x50-byte header now doubles as the buddy freelist node, so allocated blocks keep their metadata for debugging while free blocks reuse the same storage for `buddy_next/prev`.
- **Arena descriptor.**
  ```c
  typedef struct arena {
      void* base;
      size_t size;                        // always 128 MiB
      struct arena* next;
      block_header_t* freelists[21];      // orders 7..27
  } arena_t;
  ```
- **Order/size table (excerpt).**
  | Order | Block size |
  |-------|-----------|
  | 7     | 128 B     |
  | 8     | 256 B     |
  | 9     | 512 B     |
  | …     | …         |
  | 20    | 1 MiB     |
  | 21    | 2 MiB     |
  | 27    | 128 MiB   |
- **Design notes.** Orders ≤ 20 (≤ 1 MiB) cover the bulk of libc allocations. Larger orders handle gcc/Doom workloads without immediately falling back to direct `mmap`. Direct mappings still handle requests exceeding order 27 or alignments beyond the buddy range.

### Phase 3: Arena Infrastructure (#247)
**Goal:** Bootstrap buddy system

**Key Changes:**
- Replace the single global freelist with per-order freelists stored in each arena (and optionally a global array for quick lookup). Provide helpers such as `buddy_push(order, block)` / `buddy_pop(order)` so allocation code no longer touches the legacy list.
- When `grow_heap` mmaps a 128 MiB arena, initialise an `arena_t` structure (base, size, `freelists[21]`, link into `arena_list_head`). Seed the arena by materialising an order-27 `block_header_t` at offset 0 inside the buddy payload region.
- Ensure the seeded block records `order = MAX_ORDER`, `flags = BUDDY_FREE`, and `arena = current arena`. Defer splitting to the allocation path that consumes blocks from `freelists`.
- Remove the old `free_list_head` usage in favour of order-aware insertion/removal. Existing arena metadata (base pointer, size) becomes part of the new `arena_t` so free/coalesce can locate the owning freelist quickly.

### Phase 4: Core Operations (#248)
**Goal:** Implement split and coalesce

**Critical Functions:**
```c
block_header_t* buddy_split(block_header_t *block, int target_order);
block_header_t* buddy_coalesce(block_header_t *block);
void* buddy_addr(void *block, int order);  // Calculate buddy address
```

**Status:** ✅ Completed. Arenas now maintain per-order buddy freelists, `buddy_split_to_order` splits large blocks down to a requested order while seeding right-side siblings, and `buddy_coalesce_block` merges a freed block back up through the hierarchy. Host-only regression tests (`test/test_buddy_allocator.c`) exercise both operations to ensure the order bookkeeping and freelist accounting stay consistent across splits and merges. Each arena still mmaps 128 MiB, and the allocator will keep reserving additional arenas on demand—effectively unbounded until the process exhausts VM regions or the kernel runs out of physical memory.

### Phase 5: API Integration (#249)
**Goal:** Connect to malloc/free

**Changes:**
```c
void* malloc(size_t size) {
    int order = compute_order(size + sizeof(block_header_t));
    block_header_t *block = allocate_from_order(order);
    if (!block && order <= MAX_ORDER) {
        block = buddy_split(higher_order_block, order);
    }
    if (!block) {
        // Fall back to direct mmap
    }
    return (void*)(block + 1);
}

void free(void *ptr) {
    block_header_t *block = (block_header_t*)ptr - 1;
    if (block->flags & DIRECT_MMAP) {
        munmap(block, block->size);
    } else {
        mark_free(block);
        buddy_coalesce(block);
    }
}
```

### Phase 6: Extensions (#250, #251)
**Goal:** Complete allocator features

**realloc (#250):** ✅ Done
- Buddy-managed blocks now split when shrinking and attempt in-place expansion by merging the right-hand buddy before falling back to copy+free.
- `reallocarray` rides the same path, so overflow checks feed the buddy allocator automatically.

**Direct mmap (#251):** ✅ Done
- Requests larger than the buddy ceiling or requiring alignments above 16 bytes now funnel through a single helper that over-allocates, aligns, and tracks the mapping so `free` can `munmap` correctly.
- `posix_memalign`, `memalign`, `valloc`, and `pvalloc` all ride this path, so unusual alignments no longer depend on the buddy freelists.

### Phase 7: Testing (#252)
**Goal:** Validate correctness and performance

**Test Coverage:**
- Added host regression `test/test_malloc_direct.c` validating both oversized `malloc` and high-alignment `posix_memalign` paths to ensure the shared direct-mmap helper remains correct.
- Introduced `menios_malloc_stats()` alongside host test `test/test_malloc_stats.c` so we can assert arena/direct counters without manual inspection.

- Split/coalesce unit tests
- Alignment validation (posix_memalign)
- Fragmentation stress tests
- Large allocation scenarios
- Update malloc_stress for buddy patterns
- Performance comparison vs first-fit

### Phase 8: Cleanup (#253)
**Goal:** Production-ready code

**Tasks:**
- Remove old first-fit code
- Remove split_block/coalesce_with_neighbours
- Document buddy design and rationale
- Update architecture docs
- Add code comments for buddy calculations

## Validation Criteria

The migration is complete when:

- [ ] All buddy allocator issues (#245-#253) closed
- [ ] All existing malloc/free/realloc tests pass
- [ ] malloc_stress runs without errors under heavy load
- [ ] No memory leaks detected
- [ ] Fragmentation within acceptable bounds
- [ ] Performance meets or exceeds first-fit baseline
- [ ] Code fully documented with design rationale
- [ ] Architecture docs updated

## Dependencies on Other Milestones

### GCC Milestone
**Why it needs Buddy Allocator:**
- Native compilation requires robust memory management
- Compiler/linker tools have complex allocation patterns
- Buddy allocator reduces fragmentation for long-running builds

### Doom Milestone
**Why it needs Buddy Allocator:**
- Game engines stress memory allocator heavily
- Frequent allocation/deallocation of various sizes
- Buddy system's O(1) operations critical for real-time performance
- Alignment guarantees important for graphics/audio buffers

## Future Enhancements

Once buddy allocator is stable, consider:

1. **Slab Allocator Layer**
   - Fast path for common fixed sizes (16, 32, 64, 128 bytes)
   - Reduce buddy overhead for small allocations
   - Common in kernel memory management

2. **Per-CPU Arenas**
   - Reduce lock contention in threaded environments
   - Each CPU gets private arena
   - Lock-free fast path

3. **NUMA-Aware Allocation**
   - Allocate from memory local to CPU
   - Important for multi-socket systems
   - Future-proofing for SMP support

4. **Memory Compaction**
   - Move allocations to reduce fragmentation
   - Requires moving GC or cooperation from applications
   - Advanced feature for later

## References

- **Linux Kernel Buddy Allocator:** `mm/page_alloc.c`
- **FreeBSD UMA:** Universal Memory Allocator design
- **Classic Paper:** "The Buddy System" by Kenneth Knowlton (1965)
- **Modern Analysis:** "Dynamic Storage Allocation: A Survey and Critical Review" by Wilson et al.

---

**Last Updated:** 2025-10-14
**Milestone:** [Buddy Allocator](https://github.com/pbalduino/menios/milestone/4)
**See Also:**
- [Road to Shell](road_to_shell.md)
- [Road to GCC](road_to_gcc.md)
- [Road to Doom](road_to_doom.md)
