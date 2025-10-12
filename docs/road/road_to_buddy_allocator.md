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
| #245 | Survey Current Heap Implementation | 🔄 Open | High | 2-3 days |
| #246 | Define Buddy Allocator Orders and Configuration | 🔄 Open | High | 1-2 days |
| #247 | Rewrite Arena Setup for Buddy Allocator | 🔄 Open | High | 3-4 days |
| #248 | Implement Buddy Split and Coalesce Operations | 🔄 Open | Critical | 4-5 days |
| #249 | Integrate Buddy Allocator with malloc/free | 🔄 Open | Critical | 3-4 days |
| #250 | Adapt realloc/reallocarray for Buddy Allocator | 🔄 Open | High | 2-3 days |
| #251 | Update Direct mmap Path for Large Allocations | 🔄 Open | Medium | 2 days |
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
typedef struct buddy_block {
    uint32_t order;           // Block order (k for size 2^k)
    uint32_t flags;           // FREE, USED, DIRECT_MMAP
    struct buddy_block *next; // Freelist linkage
    struct buddy_block *prev;
} buddy_block_t;
```

### Arena Structure

```c
typedef struct arena {
    void *base;                           // Start address
    size_t size;                          // Total arena size
    buddy_block_t *freelists[NUM_ORDERS]; // Per-order freelists
    struct arena *next;                   // Arena chain
} arena_t;
```

## Implementation Phases

### Phase 1: Survey (#245)
**Goal:** Complete understanding of current allocator

**Deliverables:**
- Documentation of `grow_heap` arena creation
- Header layout and freelist structure diagram
- List of arbitrary-size assumptions
- Migration risk assessment

### Phase 2: Design (#246)
**Goal:** Define buddy allocator parameters

**Deliverables:**
- MIN_ORDER, MAX_ORDER, ARENA_SIZE constants
- Block metadata structure
- Arena structure with freelists
- Order-to-size mapping table

### Phase 3: Arena Infrastructure (#247)
**Goal:** Bootstrap buddy system

**Key Changes:**
- Replace single freelist with order-segregated freelists
- Seed initial arena as highest-order block
- Update arena growth to maintain buddy invariants

### Phase 4: Core Operations (#248)
**Goal:** Implement split and coalesce

**Critical Functions:**
```c
buddy_block_t* buddy_split(buddy_block_t *block, int target_order);
buddy_block_t* buddy_coalesce(buddy_block_t *block);
void* buddy_addr(void *block, int order);  // Calculate buddy address
```

### Phase 5: API Integration (#249)
**Goal:** Connect to malloc/free

**Changes:**
```c
void* malloc(size_t size) {
    int order = compute_order(size + sizeof(buddy_block_t));
    buddy_block_t *block = allocate_from_order(order);
    if (!block && order <= MAX_ORDER) {
        block = buddy_split(higher_order_block, order);
    }
    if (!block) {
        // Fall back to direct mmap
    }
    return (void*)(block + 1);
}

void free(void *ptr) {
    buddy_block_t *block = (buddy_block_t*)ptr - 1;
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

**realloc (#250):**
- Check if current order provides enough space
- Check if buddy of next order is free for growth
- Fall back to allocate + copy

**Direct mmap (#251):**
- Handle allocations > MAX_ORDER
- Handle unusual alignment via posix_memalign
- Mark with DIRECT_MMAP flag

### Phase 7: Testing (#252)
**Goal:** Validate correctness and performance

**Test Coverage:**
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
