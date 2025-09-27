#include <kernel/atomic.h>
#include <assert.h>
#include <stdio.h>

int main(void) {
    atomic32_t counter = { .value = 0 };
    assert(atomic_load32(&counter, memory_order_relaxed) == 0);

    atomic_store32(&counter, 2, memory_order_release);
    assert(atomic_load32(&counter, memory_order_acquire) == 2);

    uint32_t prev = atomic_fetch_add32(&counter, 3, memory_order_acq_rel);
    assert(prev == 2);
    assert(atomic_load32(&counter, memory_order_relaxed) == 5);

    uint32_t expected = 5;
    bool swapped = atomic_compare_exchange32(&counter, &expected, 9,
                                             memory_order_acq_rel,
                                             memory_order_relaxed);
    assert(swapped);
    assert(atomic_load32(&counter, memory_order_relaxed) == 9);

    atomic64_t wide = { .value = 0 };
    atomic_store64(&wide, 1ULL << 40, memory_order_release);
    assert(atomic_load64(&wide, memory_order_acquire) == (1ULL << 40));

    uint64_t prev64 = atomic_fetch_or64(&wide, 0xFF, memory_order_acq_rel);
    assert(prev64 == (1ULL << 40));
    assert((atomic_load64(&wide, memory_order_relaxed) & 0xFF) == 0xFF);

    (void)prev64;
    (void)swapped;

    printf("atomic tests passed\n");
    return 0;
}
