# Memory Allocation Code Analysis Report

**Date**: 2025-10-14
**Analyzed Files**:
- `user/libc/stdlib.c` - Userland buddy allocator
- `src/kernel/mem/kmalloc.c` - Kernel heap allocator
- `include/kernel/heap.h` - Kernel heap structures

---

## 🔴 **CRITICAL ISSUES**

### 1. **Buddy Allocator: Double-Free Detection Is a Silent No-Op** (Security: High) — [Issue #266](https://github.com/pbalduino/menios/issues/266)
**Location**: `stdlib.c:490-505`
```c
static void buddy_release_block(block_header_t* block) {
  if(block == NULL || block->arena == NULL) {
    return;
  }

  if(block->buddy_flags & BUDDY_FLAG_FREE) {
    return;  // << no logging or errno propagation
  }
  ...
}
```
**Problem**: Contrary to earlier notes in this file, the production code does **not** assert or log on double-free. The guard simply returns, leaving the caller unaware that it attempted to free the same block twice. That masks heap corruption bugs and offers no telemetry when double-free attempts happen in the field.

**Impact**: Silent failures make allocator misuse hard to debug. In multithreaded code (once introduced) a racing double-free would go unnoticed, potentially leaving stale pointers in higher-level caches/slabs.

**Recommended Fix**:
- In development/test builds, convert the guard into an assertion or panic.
- In production builds, at minimum emit a rate-limited warning and bump a diagnostic counter exposed through `menios_malloc_stats()` so tooling can spot the corruption attempt.

**Status (2025-10-14)**: ✔️ Addressed. `buddy_release_block()` now emits a diagnostic, sets `errno = EINVAL`, and aborts under `MENIOS_HOST_TEST`, preventing silent double-frees.

---

### 2. **Use-After-Free Risk in `buddy_coalesce_block`** (Security: Critical) — [Issue #267](https://github.com/pbalduino/menios/issues/267)
**Location**: `stdlib.c:281-283`
```c
if(buddy->buddy_offset < block->buddy_offset) {
    block = buddy;  // Pointer reassignment after buddy_freelist_remove
}
```
**Problem**: After `buddy_freelist_remove(arena, buddy)` at line 279, the `buddy` pointer's metadata is modified (flags cleared, next/prev set to NULL). Then we potentially use `buddy->buddy_offset` to compare and swap pointers. While the offset field itself isn't modified by the remove operation, this pattern is fragile and could break if the remove function is ever changed to zero out more fields.

**Impact**: If `buddy_freelist_remove` is ever modified to clear `buddy_offset` or other fields, this code will use stale data, leading to incorrect buddy merging and heap corruption.

**Recommended Fix**: Cache `buddy->buddy_offset` before calling `buddy_freelist_remove()`:
```c
uintptr_t buddy_off = buddy->buddy_offset;
buddy_freelist_remove(arena, buddy);
if(buddy_off < block->buddy_offset) {
    block = buddy;
}
```

---

### 3. **Missing NULL Check in `grow_heap`** (Reliability: High) — [Issue #268](https://github.com/pbalduino/menios/issues/268)
**Location**: `stdlib.c:555-560`
```c
block_header_t* root = buddy_materialize_block(arena, 0u, BUDDY_MAX_ORDER);
if(root == NULL) {
    munmap(mapping, mapping_size);
    errno = ENOMEM;
    return -1;
}
```
**Problem**: If `buddy_materialize_block` returns NULL, the arena is already linked into `arena_list_head` (line 549) but never unlinked before returning. This creates a dangling pointer in the global arena list pointing to unmapped memory.

**Impact**: Subsequent allocations will traverse the arena list and dereference the dangling pointer, causing a segfault or heap corruption.

**Recommended Fix**: Unlink the arena before cleanup:
```c
block_header_t* root = buddy_materialize_block(arena, 0u, BUDDY_MAX_ORDER);
if(root == NULL) {
    // Unlink arena from list
    if(arena->next != NULL) {
        arena->next->prev = arena->prev;
    }
    if(arena->prev != NULL) {
        arena->prev->next = arena->next;
    } else {
        arena_list_head = arena->next;
    }

    munmap(mapping, mapping_size);
    errno = ENOMEM;
    return -1;
}
```

---

### 4. **Integer Overflow in `buddy_order_size`** (Security: Medium)
**Location**: `stdlib.c:120-122`
```c
static inline size_t buddy_order_size(uint32_t order) {
    return (size_t)1u << order;
}
```
**Problem**: When `order >= 64` on a 64-bit system (or `>= 32` on 32-bit), this causes undefined behavior due to shift overflow. The result wraps around or produces garbage.

**Current Mitigation**: The code uses `buddy_order_valid()` checks in most places, which limits orders to 7-27. However, if this function is ever called with an invalid order due to a bug, the UB could be exploited.

**Impact**: Memory corruption, incorrect size calculations, or crashes.

**Recommended Fix**: Add compile-time bounds check:
```c
_Static_assert(BUDDY_MAX_ORDER < 64, "BUDDY_MAX_ORDER must fit in size_t shift");
_Static_assert(BUDDY_MAX_ORDER < 32 || sizeof(size_t) >= 8, "BUDDY_MAX_ORDER requires 64-bit size_t");

static inline size_t buddy_order_size(uint32_t order) {
    assert(buddy_order_valid(order));  // Runtime check in debug builds
    return (size_t)1u << order;
}
```

---

### 5. **Direct mmap Alignment Calculation Error** (Correctness: High) — [Issue #268](https://github.com/pbalduino/menios/issues/268)
**Location**: `stdlib.c:590-592`
```c
uintptr_t base = (uintptr_t)mapping + sizeof(block_header_t);
uintptr_t aligned_addr = align_up(base, alignment);
block_header_t* header = (block_header_t*)(aligned_addr - sizeof(block_header_t));
```
**Problem**: This assumes that after aligning the payload address, there's still room for a full `block_header_t` immediately before it. However, if `alignment` is large (e.g., 2 MB for huge pages), and the `mmap` base happens to be poorly aligned, the header could overlap with unmapped memory or the mapping base itself.

**Example Failure Case**:
- `mapping = 0x1000`
- `alignment = 2MB = 0x200000`
- `base = 0x1000 + 0x60 = 0x1060`
- `aligned_addr = align_up(0x1060, 0x200000) = 0x200000`
- `header = 0x200000 - 0x60 = 0x1fffa0`

If `total < 0x1fffa0 - 0x1000`, the header is outside the mapped region!

**Analysis of Current Code**: The calculation on line 578 is:
```c
size_t extra = alignment + sizeof(block_header_t);
size_t total = padded + extra;
```
This **should** guarantee enough space: we allocate `padded + alignment + header_size`, so in the worst case where we need to align forward by nearly `alignment` bytes, we still have room for the header. However, the logic is non-obvious and fragile.

**Recommended Fix**: Add assertions and clearer documentation:
```c
void* mapping = mmap(..., total, ...);
if(mapping == MAP_FAILED) { ... }

uintptr_t base = (uintptr_t)mapping + sizeof(block_header_t);
uintptr_t aligned_addr = align_up(base, alignment);
block_header_t* header = (block_header_t*)(aligned_addr - sizeof(block_header_t));

// Verify header is within the mapped region
assert(header >= mapping);
assert((uintptr_t)header + sizeof(block_header_t) + padded <= (uintptr_t)mapping + total);
```

---

### 6. **Kernel Heap: Magic Number Check Bypassed** (Security: Medium) — [Issue #271](https://github.com/pbalduino/menios/issues/271)
**Location**: `kmalloc.c:640-643`
```c
if(node->magic != HEAP_MAGIC) {
    serial_printf("kfree: invalid pointer %p\n", ptr);
    spinlock_unlock(&heap_lock);
    return;  // Silent return after logging
}
```
**Problem**: Invalid frees are only logged but don't panic or assert. An attacker or buggy code could repeatedly call `kfree()` with crafted pointers to:
1. Spam kernel logs (DoS via log flooding)
2. Probe heap layout by observing which addresses trigger the message
3. Potentially bypass security checks if the magic number is guessable

**Impact**: In production, this enables silent failures that mask bugs. In hostile environments, it's an information leak.

**Recommended Fix**: Escalate severity based on build type:
```c
if(node->magic != HEAP_MAGIC) {
    serial_printf("kfree: invalid pointer %p (magic=0x%x)\n", ptr, node->magic);
#ifdef DEBUG
    panic("kfree: heap corruption detected");
#else
    // In production, rate-limit or increment corruption counter
    spinlock_unlock(&heap_lock);
    return;
#endif
}
```

---

## 🟡 **PERFORMANCE ISSUES**

### 7. **Buddy Allocator: Linear Search Across All Arenas** (Performance: Medium) — [Issue #269](https://github.com/pbalduino/menios/issues/269)
**Location**: `stdlib.c:444-452`
```c
for(int attempt = 0; attempt < 2; ++attempt) {
    for(arena_header_t* arena = arena_list_head; arena != NULL; arena = arena->next) {
        for(uint32_t current = order; current <= BUDDY_MAX_ORDER; ++current) {
            block_header_t* candidate = buddy_freelist_pop(arena, current);
```
**Problem**: Triple nested loop searches all arenas, all orders, twice if the first pass fails and triggers heap growth. For a workload with many arenas, this becomes O(A × O × 2) where A = arena count, O = order count (21).

**Impact**: Noticeable slowdown when many arenas exist. For example:
- 10 arenas × 21 orders × 2 attempts = 420 freelist checks per allocation
- Each check walks the freelist head, which is O(1) for pop but the outer loops dominate

**Measurement**: Run `malloc_stress` with varied workloads to quantify the slowdown.

**Recommended Fixes** (in priority order):
1. **Best-fit arena heuristic**: Track which arena was last successful and try it first:
   ```c
   static arena_header_t* last_successful_arena = NULL;

   // Try last successful arena first
   if(last_successful_arena != NULL) {
       for(uint32_t current = order; current <= BUDDY_MAX_ORDER; ++current) {
           block_header_t* candidate = buddy_freelist_pop(last_successful_arena, current);
           if(candidate != NULL) {
               return buddy_split_to_order(candidate, order);
           }
       }
   }

   // Then try all arenas (original loop)
   ```

2. **Global freelists**: Maintain global freelists that aggregate across arenas (trade memory for speed):
   ```c
   static block_header_t* global_buddy_freelists[BUDDY_ORDER_COUNT];

   // On push: add to both arena freelist and global freelist
   // On pop: check global freelist first, then fall back to per-arena search
   ```
   **Tradeoff**: Uses more memory (extra pointers per block), but reduces search to O(O) instead of O(A × O).

3. **Arena-order bitmap**: Maintain a bitmap of which arenas have free blocks at which orders:
   ```c
   typedef struct arena_header {
       ...
       uint32_t free_orders_bitmap;  // Bit i set if freelist[i] is non-empty
   } arena_header_t;
   ```
   Then skip arenas with no free blocks at the desired order.

---

### 8. **Kernel Heap: O(n²) Coalescing on Free** (Performance: High)
**Location**: `kmalloc.c:655, 668`
```c
heap_node_p prev = heap_find_previous(node);  // O(n) scan
...
heap_merge_forward(node);  // O(n) in worst case
```
**Problem**: Every `kfree()` call does a full O(n) traversal in `heap_find_previous()` to find the node's predecessor in the singly-linked freelist. Then `heap_merge_forward()` scans forward to merge contiguous free blocks.

**Impact**: Severe performance degradation with many small allocations. Consider:
- 1000 allocations, each 64 bytes
- Each free scans up to 1000 nodes to find predecessor
- Total: O(n²) = 1,000,000 operations for freeing 1000 blocks

**Measurement**: In kernel with heavy kfree activity (e.g., network packet processing, process spawning), this can dominate CPU time.

**Recommended Fix**: Use **doubly-linked lists** for O(1) predecessor lookup:
```c
typedef struct heap_node_t {
    uint32_t             magic;
    uint8_t              status;
    uint32_t             size;
    struct heap_node_t*  next;
    struct heap_node_t*  prev;  // ADD THIS
    uint8_t              data[];
} heap_node_t;
```

**Required Changes**:
1. Update `heap_grow()` to set `node->prev = previous_tail` (line 376)
2. Update `heap_split_node()` to maintain `prev` pointers (line 405)
3. Replace `heap_find_previous()` with direct `node->prev` access in `kfree()` (line 655)
4. Update all list manipulation to maintain bidirectional links

**Estimated Impact**: Reduces `kfree()` from O(n) to O(1) for the predecessor lookup, keeping only the O(k) merge-forward cost where k = number of contiguous free blocks (typically small).

---

### 9. **Buddy Allocator: Freelist Linear Search for Coalescing** (Performance: Medium)
**Location**: `stdlib.c:218-223`
```c
for(block_header_t* node = arena->buddy_freelists[index]; node != NULL; node = node->buddy_next) {
    if(node->buddy_offset == offset) {
        return node;
    }
}
```
**Problem**: Finding the buddy during coalescing requires a linear search through the freelist for that order. With many free blocks at the same order, this becomes slow.

**Impact**: Coalescing is O(F) where F = number of free blocks at that order. This defeats the O(log n) buddy advantage. For example:
- 100 free blocks at order 10 (1 KB)
- Each coalesce scans up to 100 blocks
- Frequent free/realloc patterns hit this repeatedly

**Recommended Fixes** (in priority order):
1. **Bitmap approach** (simplest): Maintain a bitmap tracking which offsets are free at each order:
   ```c
   typedef struct arena_header {
       ...
       uint64_t free_bitmaps[BUDDY_ORDER_COUNT][MAX_BLOCKS/64];  // Bit = 1 if free
   } arena_header_t;

   // On buddy_freelist_push: set bit for offset
   // On buddy_freelist_remove: clear bit
   // On coalesce: check bit instead of linear search
   ```
   **Tradeoff**: Uses O(arena_size / min_block_size / 8) bytes per arena. For 128 MiB arena with 128B min block, that's ~128 KB of bitmaps per arena.

2. **Hash table**: Use a hash table mapping offset → block:
   ```c
   #define FREELIST_HASH_SIZE 256

   typedef struct arena_header {
       ...
       block_header_t* free_hash[BUDDY_ORDER_COUNT][FREELIST_HASH_SIZE];
   } arena_header_t;

   static inline size_t hash_offset(uintptr_t offset) {
       return (offset >> 7) % FREELIST_HASH_SIZE;  // Divide by min block size, then mod
   }
   ```
   Reduces search from O(F) to O(F/256) average case.

3. **Sorted freelist**: Keep each freelist sorted by offset. Binary search reduces lookup to O(log F). However, insertion becomes O(F), so this only helps if coalescing is more frequent than allocation.

---

## 🟠 **DESIGN CONCERNS**

### 10. **Buddy Allocator: Unbounded Arena Growth** (Resource: High)
**Location**: `stdlib.c:507-564`
**Problem**: Each call to `grow_heap()` allocates a new **128 MiB arena** via `mmap()`. There's no upper limit on the number of arenas. A runaway allocation loop could exhaust virtual memory or physical RAM.

**Current Situation**:
- Process can allocate unlimited 128 MiB chunks until kernel refuses `mmap()`.
- On a system with 4 GB RAM, this could create 30+ arenas before OOM.
- Virtual address space on x86-64 is 48-bit = 256 TB, so VA exhaustion is unlikely, but physical RAM is the real constraint.

**Impact**:
- **DoS attack**: Malicious process allocates until system OOM kills critical processes.
- **Resource exhaustion**: Bug in userland (e.g., infinite loop with malloc) crashes the system instead of just the process.

**Recommended Fixes** (choose one or combine):
1. **Hard arena limit**:
   ```c
   #define MAX_ARENA_COUNT 32  // 32 × 128 MiB = 4 GB max
   static size_t arena_count = 0;

   static int grow_heap(size_t size) {
       if(arena_count >= MAX_ARENA_COUNT) {
           errno = ENOMEM;
           return -1;
       }
       ...
       arena_count++;
   }
   ```

2. **Exponential backoff** (shrink arena size after threshold):
   ```c
   static size_t current_arena_size = ARENA_INITIAL_SIZE;

   static int grow_heap(size_t size) {
       size_t mapping_size = current_arena_size;
       if(arena_count > 8) {
           mapping_size /= 2;  // Shrink to 64 MiB after 8 arenas
       }
       if(mapping_size < PAGE_SIZE) {
           errno = ENOMEM;
           return -1;
       }
       ...
   }
   ```

3. **Kernel-enforced rlimit** (requires kernel support for `RLIMIT_AS`):
   ```c
   // In kernel: track per-process virtual memory usage
   // Reject mmap if process exceeds its limit
   ```
   **Best long-term solution** but requires significant kernel work.

---

### 11. **Kernel Heap: Region Descriptor Pool is Fixed Size** (Scalability: Medium)
**Location**: `kmalloc.c:15, 38`
```c
#define HEAP_REGION_CAP     64UL
...
static heap_region_t heap_region_entries[HEAP_REGION_CAP];
```
**Problem**: The kernel can only track 64 heap regions. Once exhausted, `heap_register_region()` fails (line 166) and the system can't grow the heap further, even if RAM is available.

**Impact**: Hard limit on kernel heap scalability. Scenarios that hit this:
- Long-running system with many small regions (if heap grows frequently in small chunks)
- Fragmentation leads to many disjoint regions
- Once limit is hit, kernel can't allocate memory even if 90% of RAM is free

**Measurement**: Add logging to track `arena_count` and warn at 50/64 regions.

**Recommended Fixes**:
1. **Increase limit** (easiest):
   ```c
   #define HEAP_REGION_CAP     256UL  // 4x increase
   ```
   **Tradeoff**: Uses more static memory (~8 KB → ~32 KB), but negligible for kernel.

2. **Dynamic region list** (better long-term):
   Replace the static array with a dynamically allocated linked list. However, this creates a bootstrapping problem: we need the heap to allocate region descriptors, but the heap needs region descriptors to grow. Solutions:
   - Reserve first N regions statically, then dynamically allocate more
   - Use a separate, simple allocator for region descriptors (e.g., page-granular allocator)

3. **Region merging**: When adjacent regions exist (e.g., two 4 KB regions in a row), merge them into one descriptor. This reduces descriptor usage but complicates bookkeeping.

---

### 12. **Buddy Allocator: No Memory Limit Per Process** (Security: Low)
**Problem**: A single userland process can allocate until the system runs out of memory. There's no per-process quota or cgroup-style limit.

**Impact**:
- One runaway process can DoS the entire system
- No isolation between processes in multi-user environment

**Recommended Fix**: Add process memory limits (requires kernel support):
1. **RLIMIT_AS** (address space limit):
   ```c
   // In kernel: track per-process VM usage
   struct process {
       ...
       size_t vm_used;
       size_t vm_limit;  // Set via setrlimit(RLIMIT_AS, ...)
   };

   // In mmap syscall:
   if(current->vm_used + size > current->vm_limit) {
       return -ENOMEM;
   }
   ```

2. **RLIMIT_DATA** (heap data segment limit):
   Track only brk/mmap(MAP_ANONYMOUS) allocations, not code/stack.

**Note**: This is a kernel-level feature, not a libc fix. Requires significant kernel work (issue #262 is related: better fault handling).

---

## 🔵 **CODE QUALITY ISSUES**

### 13. **Kernel Heap: `heap_node_t` Struct Padding Issues** (Portability: Low)
**Location**: `heap.h:23-29`
```c
typedef struct heap_node_t {
  uint32_t             magic;  // 4 bytes at offset 0
  uint8_t              status; // 1 byte at offset 4
  uint32_t             size;   // 4 bytes at offset 8 (!)
  struct heap_node_t*  next;   // 8 bytes at offset 16
  uint8_t              data[];
} heap_node_t;
```
**Problem**: The struct has implicit padding:
- After `status` (offset 4, size 1), compiler inserts **3 bytes padding** to align `size` (uint32_t requires 4-byte alignment).
- After `size` (offset 8, size 4), compiler inserts **4 bytes padding** to align `next` (pointer requires 8-byte alignment on 64-bit).
- Actual layout: `magic` (0-3), `status` (4), padding (5-7), `size` (8-11), padding (12-15), `next` (16-23), `data` (24+)
- `HEAP_HEADER_SIZE = offsetof(heap_node_t, data) = 24` bytes (not 17).

**Impact**:
- Not a bug per se (the code correctly uses `offsetof`), but wastes 7 bytes per allocation.
- On a system with 10,000 allocations, that's 70 KB wasted.
- More importantly, the layout is non-obvious and could confuse future maintainers.

**Recommended Fix**: Reorder fields to minimize padding:
```c
typedef struct heap_node_t {
  struct heap_node_t*  next;   // 8 bytes at offset 0 (aligned)
  uint32_t             magic;  // 4 bytes at offset 8
  uint32_t             size;   // 4 bytes at offset 12 (no padding needed!)
  uint8_t              status; // 1 byte at offset 16
  uint8_t              padding[7];  // Explicit padding to 24 bytes for alignment
  uint8_t              data[];
} heap_node_t;
```
**Result**: Same 24-byte size, but explicit and self-documenting. Add static assert:
```c
_Static_assert(sizeof(heap_node_t) == 24, "heap_node_t size changed");
_Static_assert(offsetof(heap_node_t, data) == 24, "heap_node_t data offset changed");
```

---

### 14. **Buddy Allocator: Excessive `fprintf` Debugging in Production** (Performance: Low)
**Location**: `stdlib.c:527-533`
```c
fprintf(stderr,
        "grow_heap: mapping=%p size=%zu payload=%zu header=%zu page=%zu\n",
        mapping, mapping_size, buddy_bytes, header_size, page);
```
**Problem**: This `fprintf` runs on **every arena allocation** (every 128 MiB of heap growth), even in production builds. Logging to stderr requires:
1. A `write()` syscall (context switch)
2. Kernel copying data to serial/console
3. Potentially blocking I/O if stderr is a slow device

**Impact**: Slows down arena allocation, which is already on the critical path. Negligible for occasional use, but if a workload rapidly grows the heap (e.g., loading large files), this adds up.

**Recommended Fix**: Guard with debug macro:
```c
#ifdef MENIOS_MALLOC_DEBUG
fprintf(stderr,
        "grow_heap: mapping=%p size=%zu payload=%zu header=%zu page=%zu\n",
        mapping, mapping_size, buddy_bytes, header_size, page);
#endif
```

**Alternative**: Use a debug level that can be toggled at runtime:
```c
static int malloc_debug_level = 0;  // 0 = off, 1 = errors, 2 = warnings, 3 = info

void menios_malloc_set_debug(int level) {
    malloc_debug_level = level;
}

// In grow_heap:
if(malloc_debug_level >= 3) {
    fprintf(stderr, "grow_heap: ...\n", ...);
}
```

---

### 15. **Kernel Heap: `heap_compactor` Zeros Free Memory Unnecessarily** (Performance: Medium)
**Location**: `kmalloc.c:689`
```c
memzero(node->data, node->size);
```
**Problem**: The compactor zeros out free node payloads, which takes time proportional to total free memory. For example:
- System has 256 MB kernel heap
- 128 MB is free (fragmented across many nodes)
- `heap_compactor()` zeros all 128 MB, which takes ~10-50 ms depending on CPU speed

**Purpose**: Zeroing is a security feature to prevent:
1. **Information leaks**: Old data in freed memory could be read by next allocator
2. **Use-after-free exploitation**: Old pointers/data could be leveraged by attackers

**Impact**:
- Compactor runs slowly, potentially blocking other kernel operations if called with interrupts disabled or spinlock held (it is: line 676).
- On a busy system, this could cause latency spikes.

**Recommended Fixes**:
1. **Make zeroing optional** (off by default):
   ```c
   #ifdef MENIOS_SECURE_HEAP
   #define HEAP_ZERO_ON_FREE 1
   #else
   #define HEAP_ZERO_ON_FREE 0
   #endif

   if(HEAP_ZERO_ON_FREE) {
       memzero(node->data, node->size);
   }
   ```

2. **Zero on allocation instead** (lazy zeroing):
   Instead of zeroing in compactor, zero in `kmalloc` before returning to caller:
   ```c
   void* kmalloc(size_t size) {
       ...
       void* ptr = (void*)node->data;
       memzero(ptr, size);  // Only zero requested size, not full node
       return ptr;
   }
   ```
   **Tradeoff**: Slightly slower allocations, but only pays cost when memory is actually used.

3. **Zero in background**: Run compactor in a low-priority kernel thread instead of in critical path. This requires more kernel infrastructure (thread scheduler, work queues).

---

## 🟢 **IMPROVEMENT OPPORTUNITIES**

### 16. **Buddy Allocator: No Free Block Coalescing Across Arena Boundaries**
**Observation**: Each arena is independent. If two adjacent arenas are fully free, they cannot be merged back into the OS.

**Example Scenario**:
- Process allocates 500 MB (4 arenas)
- Process frees 384 MB (3 full arenas)
- Those 3 arenas remain mapped but unused
- Process continues running for hours with 384 MB "dead weight"

**Impact**: Long-running processes accumulate "dead" arenas that hold a single small allocation, wasting virtual address space and potentially physical RAM (if pages are still resident).

**Recommended Fix**: Implement **arena retirement**:
```c
static void arena_try_retire(arena_header_t* arena) {
    // Check if arena is fully free
    bool fully_free = true;
    for(size_t i = 0; i < BUDDY_ORDER_COUNT; ++i) {
        if(arena->buddy_freelists[i] != NULL) {
            for(block_header_t* node = arena->buddy_freelists[i]; node != NULL; node = node->buddy_next) {
                if(node->buddy_flags & BUDDY_FLAG_USED) {
                    fully_free = false;
                    break;
                }
            }
        }
        if(!fully_free) break;
    }

    // Actually, better check: is there exactly one free block at MAX_ORDER covering the whole arena?
    fully_free = (arena->buddy_freelists[BUDDY_ORDER_COUNT-1] != NULL &&
                  arena->buddy_freelists[BUDDY_ORDER_COUNT-1]->buddy_offset == 0 &&
                  arena->buddy_freelists[BUDDY_ORDER_COUNT-1]->buddy_next == NULL);

    if(fully_free && arena != arena_list_head) {  // Keep at least one arena
        // Unlink from list
        if(arena->prev) arena->prev->next = arena->next;
        if(arena->next) arena->next->prev = arena->prev;

        // Munmap
        munmap(arena->mapping_base, arena->size);
    }
}

// Call from buddy_coalesce_block after pushing the coalesced block
```

**Tradeoff**: Adds overhead to coalescing (need to check if arena is fully free). But this only triggers when coalescing produces a MAX_ORDER block, which is rare.

---

### 17. **Kernel Heap: No NUMA Awareness**
**Observation**: All kernel heap allocations come from a single global heap, regardless of which CPU core/NUMA node is requesting memory.

**Impact**: On multi-socket systems (future):
- CPU 0 (node 0) allocates memory that happens to be physical RAM on node 1
- Every access crosses the interconnect (e.g., Intel QPI, AMD Infinity Fabric)
- Latency increases by 2-3x, bandwidth decreases

**Recommended Fix** (long-term, requires NUMA support first):
1. **Per-NUMA-node heaps**:
   ```c
   static heap_node_p heap[MAX_NUMA_NODES];

   void* kmalloc(size_t size) {
       int node = current_cpu_numa_node();
       return kmalloc_node(size, node);
   }
   ```

2. **Local allocation by default, remote fallback**:
   Try local node first, then fall back to other nodes if local is exhausted.

**Prerequisites**:
- NUMA topology detection (ACPI SRAT table)
- Per-node physical memory allocator (extend `pmm.c`)
- CPU→NUMA node mapping

---

### 18. **Buddy Allocator: No SizeClass Cache (Slab Layer)**
**Observation**: Every allocation, even tiny ones (e.g., 16 bytes), goes through the buddy system and splits large blocks. This is inefficient for common small sizes.

**Impact**: Internal fragmentation:
- 16-byte allocation uses minimum order 7 = 128 bytes
- Wastes 112 bytes (87.5% overhead!)
- For 1000 small allocations, wastes 112 KB

**Recommended Fix** (already in roadmap): Add a **slab allocator** layer on top:
```c
// Size classes for common sizes
#define SLAB_SIZES {16, 32, 64, 128, 256, 512, 1024}

typedef struct slab {
    void* blocks[64];      // Array of 64 blocks
    uint64_t free_bitmap;  // Bit i = 1 if blocks[i] is free
    struct slab* next;
} slab_t;

static slab_t* slabs[7];  // One per size class

void* malloc(size_t size) {
    if(size <= 1024) {
        int class = size_to_class(size);
        if(slabs[class] && slabs[class]->free_bitmap != 0) {
            // Fast path: allocate from slab
            int idx = __builtin_ctzll(slabs[class]->free_bitmap);
            slabs[class]->free_bitmap &= ~(1ULL << idx);
            return slabs[class]->blocks[idx];
        }
        // Slow path: allocate new slab from buddy
        allocate_slab(class);
    }

    // Fall back to buddy for large allocations
    return buddy_allocate(...);
}
```

**Benefits**:
- Eliminates internal fragmentation for small sizes
- Faster allocation (O(1) bitmap check vs O(A × O) buddy search)
- Better cache locality (slabs are dense)

**Tradeoffs**:
- More complex implementation
- Uses more memory for slab metadata
- Need to tune slab sizes based on workload

---

### 19. **Both Allocators: No Allocation Profiling**
**Observation**: Neither allocator tracks allocation sources (callsite, size histogram, peak memory, etc.).

**Impact**: Hard to debug memory leaks or identify hot paths. For example:
- "Why is the system using 2 GB of RAM?"
- "Which function is allocating the most?"
- "Is there a memory leak, or just high water mark?"

**Recommended Fix**: Add `#ifdef PROFILE` mode that records allocation metadata:
```c
#ifdef MENIOS_MALLOC_PROFILE
typedef struct alloc_record {
    void* ptr;
    size_t size;
    void* caller;  // Return address from __builtin_return_address(0)
    uint64_t timestamp;
} alloc_record_t;

static alloc_record_t alloc_log[MAX_ALLOCS];
static size_t alloc_log_idx = 0;

void* malloc(size_t size) {
    void* ptr = buddy_allocate_block(size);
    if(ptr && alloc_log_idx < MAX_ALLOCS) {
        alloc_log[alloc_log_idx++] = (alloc_record_t){
            .ptr = ptr,
            .size = size,
            .caller = __builtin_return_address(0),
            .timestamp = rdtsc()
        };
    }
    return ptr;
}

// Add API to dump alloc_log to file or stderr
void menios_malloc_dump_profile(FILE* out);
```

**Advanced Features**:
- **Stack traces**: Capture full call stack (requires unwind info)
- **Size histogram**: Track distribution of allocation sizes
- **Leak detection**: Compare alloc vs free logs
- **Flamegraphs**: Visualize which functions allocate the most

**Tradeoffs**:
- Significant memory overhead (each record ~32 bytes)
- Performance overhead (logging on every alloc/free)
- Only enable in development/debugging builds

---

### 20. **Kernel Heap: Releasing Regions Leaves Stale Virtual Mappings** (Security: Critical) — [Issue #263](https://github.com/pbalduino/menios/issues/263)
**Location**: `kmalloc.c:300-335`
```c
if(heap_region_contains_other_nodes(region, node)) {
  return;
}
...
pmm_free_pages(region->phys_base, region->page_count);
heap_unregister_region(region);
```
**Problem**: When a region becomes entirely free, the allocator returns the physical pages to the PMM but keeps the old virtual range mapped. Later, the PMM may hand those frames to another subsystem while the stale heap virtual addresses remain writable. Any dangling pointer into the released region can now scribble over unrelated kernel data.

**Impact**: Use-after-free becomes a cross-subsystem memory corruption primitive. The kernel effectively leaks writable aliases to freshly allocated physical memory, which is a severe security bug.

**Recommended Fix**:
1. Walk the region and call `pmm_unmap_page`/`pmm_unmap_page_in_root` for every page before freeing the frames.
2. Optionally return the virtual span to an address-space allocator so it can be reused for future `heap_grow` calls.
3. Add assertions to ensure we never see mapped-but-untracked regions during diagnostics.

---

### 21. **Kernel Heap: Partial Mapping Failures Leak Aliases** (Reliability: High) — [Issue #264](https://github.com/pbalduino/menios/issues/264)
**Location**: `kmalloc.c:51-68`
```c
for(size_t page = 0; page < page_count; page++) {
  ...
  if(!pmm_map_page(vaddr, phys, true, false)) {
    serial_printf("heap_map_region: map failed at %lx\n", (unsigned long)vaddr);
    return NULL;  // early exit, previously mapped pages stay mapped
  }
}
```
**Problem**: If a mid-loop `pmm_map_page` fails, the earlier pages remain mapped while `heap_map_region` returns `NULL`. `heap_grow()` then frees the physical run, leaving a partially mapped virtual range pointing at freed frames—the same aliasing flaw as Issue 20, but triggered by transient failures instead of normal teardown.

**Recommended Fix**: Add rollback logic that unmaps any pages mapped in the current call before returning `NULL`, and avoid returning the physical pages to the PMM until the mapping succeeds.

---

### 22. **Kernel Heap: Monotonic Virtual Address Consumption** (Resource: High)
**Location**: `kmalloc.c:42-71`
**Problem**: `heap_next_vaddr` only increases; released regions never return their virtual span. A workload that repeatedly allocates, frees, and then grows the heap will eventually exhaust the 64 MiB heap window even though plenty of RAM is available.

**Impact**: Once `KHEAP_LIMIT` is reached the allocator can no longer grow, causing `kmalloc` failures even with abundant physical memory.

**Recommended Fix**: Maintain a free list of virtual ranges or switch to `vmem`/`vmap`-style management so released regions can be recycled.

---

### 23. **User Buddy Allocator: Global State Is Unsynchronised** (Correctness: Critical) — [Issue #265](https://github.com/pbalduino/menios/issues/265)
**Location**: `stdlib.c:106-775`
**Problem**: All allocator globals (`arena_list_head`, freelists, `direct_allocation_count`, `direct_total_bytes`) are mutated without any locking. In a multi-threaded process, concurrent `malloc`/`free` calls will corrupt the freelists almost immediately. `menios_malloc_stats()` also walks the lists without coordination and will race with allocations even in single-threaded code once signals/threads arrive.

**Impact**: As soon as pthreads land, the allocator becomes unusable. Even today, signal handlers or `atexit` callbacks running concurrently could trigger corruption.

**Recommended Fix**:
1. Introduce a global allocator lock (e.g., `static pthread_mutex_t malloc_lock;`) and guard every public entry point plus `menios_malloc_stats()`.
2. Longer term, move to per-arena locks or thread-caching slabs to reduce contention.
3. Extend tests (`test_malloc_stats.c`, stress suites) to exercise concurrent callers once locking exists.

---

## 📊 **SUMMARY TABLE**

| # | Issue | Severity | Type | Fix Effort | Status |
|---|-------|----------|------|------------|--------|
| 1 | Double-free silent failure | High | Security | Low | 🔴 OPEN |
| 2 | Use-after-free in coalesce | Critical | Security | Low | 🔴 OPEN |
| 3 | Missing NULL check in grow_heap | High | Reliability | Low | 🔴 OPEN |
| 4 | Integer overflow in buddy_order_size | Medium | Security | Low | 🔴 OPEN |
| 5 | Direct mmap alignment calculation | High | Correctness | Medium | 🔴 OPEN |
| 6 | Kernel heap magic bypass | Medium | Security | Low | 🔴 OPEN |
| 7 | Buddy linear search across arenas | Medium | Performance | High | 🔴 OPEN |
| 8 | Kernel O(n²) coalescing | High | Performance | Medium | 🔴 OPEN |
| 9 | Buddy freelist linear search | Medium | Performance | High | 🔴 OPEN |
| 10 | Unbounded arena growth | High | Resource | Low | 🔴 OPEN |
| 11 | Fixed region descriptor pool | Medium | Scalability | Low | 🔴 OPEN |
| 12 | No per-process memory limit | Low | Security | High | 🔴 OPEN |
| 13 | Kernel struct padding | Low | Portability | Low | 🔴 OPEN |
| 14 | Excessive debug logging | Low | Performance | Trivial | 🔴 OPEN |
| 15 | Compactor zeroing overhead | Medium | Performance | Low | 🔴 OPEN |
| 16 | No arena retirement | Medium | Efficiency | Medium | 🔴 OPEN |
| 17 | No NUMA awareness | Low | Performance | High | 🔴 OPEN |
| 18 | No slab layer | Medium | Efficiency | High | 🔴 OPEN |
| 19 | No allocation profiling | Low | Debuggability | Medium | 🔴 OPEN |
| 20 | Kernel heap leaves stale mappings on region release | Critical | Security | Medium | ✅ CLOSED ([#263](https://github.com/pbalduino/menios/issues/263)) |
| 21 | Kernel heap partial map rollback missing | High | Reliability | Medium | ✅ CLOSED ([#264](https://github.com/pbalduino/menios/issues/264)) |
| 22 | Kernel heap virtual address exhaustion | High | Resource | Medium | 🔴 OPEN |
| 23 | User buddy allocator lacks locking | Critical | Correctness | Medium | ✅ CLOSED ([#265](https://github.com/pbalduino/menios/issues/265)) |

---

## 🎯 **RECOMMENDED PRIORITY ORDER**

### Immediate (Before Production Use):
1. ~~**Fix Issue #20** ([#263](https://github.com/pbalduino/menios/issues/263)) — stale virtual mappings after region release – security-critical aliasing bug~~ ✅ Done (region pages are now unmapped before frames are released)
2. ~~**Fix Issue #21** ([#264](https://github.com/pbalduino/menios/issues/264)) — partial map rollback – prevents the same aliasing on failure paths~~ ✅ Done (mapping failures now roll back and clean partial mappings)
3. ~~**Fix Issue #23** ([#265](https://github.com/pbalduino/menios/issues/265)) — allocator locking – user-mode heap is currently unsafe for concurrency~~ ✅ Done (global allocator lock now guards all heap operations)
4. **Fix Issue #3** (grow_heap NULL handling) – avoids dangling arenas after allocation failures
5. **Fix Issue #5** (direct mmap alignment) – plugs subtle corruption for high-alignment callers

### High Priority (Next Sprint):
6. **Fix Issue #22** (virtual address recycling for kmalloc) – prevents premature heap exhaustion
7. **Fix Issue #10** (unbounded arena growth) – caps userland heap expansion
8. **Fix Issue #7** (buddy arena linear search) – improves scaling before adding slab caches
9. **Fix Issue #8** (kernel O(n²) coalescing) – removes pathological frees
10. **Fix Issue #11** (region descriptor limit) – avoids kmalloc hard-failures under churn

### Medium Priority (Technical Debt):
11. **Fix Issue #9** (freelist linear search) – tighten buddy coalesce cost
12. **Fix Issue #15** (compactor zeroing) – decide on perf vs scrub policy
13. **Add Issue #16** (arena retirement) – reclaim unused user arenas

### Long-Term (Nice to Have):
14. **Add Issue #18** (slab layer) – already tracked in roadmap
15. **Add Issue #19** (profiling) – boosts observability
16. **Add Issue #17** (NUMA awareness) – future SMP preparedness

---

## ✅ **WHAT'S WORKING WELL**

1. **Buddy algorithm is correctly implemented** - Split/coalesce logic is sound
2. **Thread safety in kernel heap** - Spinlock usage is appropriate
3. **Comprehensive test coverage** - `test_buddy_allocator.c`, `test_malloc_direct.c`, etc. exist
4. **Diagnostics support** - `menios_malloc_stats()` provides useful metrics
5. **Direct mmap fallback** - Handles large allocations and unusual alignments
6. **Kernel heap can grow dynamically** - Good for long-running systems
7. **Double-free detection in both allocators** - Prevents some corruption (now fixed in buddy allocator ✅)
8. **Well-documented code** - Comments explain buddy algorithm clearly

---

## 📝 **TESTING RECOMMENDATIONS**

### Stress Tests to Add:
1. **Arena exhaustion test**: Allocate until hitting any limits, verify graceful ENOMEM
2. **Fragmentation test**: Alternating alloc/free patterns to measure worst-case performance
3. **Alignment test**: Verify direct mmap alignment calculation with huge alignment values
4. **Coalesce correctness**: Allocate/free in patterns designed to trigger all buddy merge paths
5. **Concurrent stress** (kernel): Multi-threaded kmalloc/kfree to verify spinlock correctness

### Fuzzing Targets:
- `malloc(random_size)` with sizes from 0 to 2 GB
- `realloc(ptr, random_size)` with random old sizes
- `free()` with invalid/double-free pointers (should not crash in production)
- `posix_memalign()` with unusual alignments (1 byte, 3 bytes, 1 GB)

---

**Last Updated**: 2025-10-14
**Reviewers**: TBD
**Related Issues**: #262 (user-mode fault handling), Buddy Allocator milestone (#245-#253 ✅ COMPLETE)
