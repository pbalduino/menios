#include <kernel/atomic.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void test_atomic32_operations(void) {
    atomic32_t counter = { .value = 0 };

    TEST_ASSERT_EQUAL_UINT32(0, atomic_load32(&counter, memory_order_relaxed));

    atomic_store32(&counter, 2, memory_order_release);
    TEST_ASSERT_EQUAL_UINT32(2, atomic_load32(&counter, memory_order_acquire));

    uint32_t prev = atomic_fetch_add32(&counter, 3, memory_order_acq_rel);
    TEST_ASSERT_EQUAL_UINT32(2, prev);
    TEST_ASSERT_EQUAL_UINT32(5, atomic_load32(&counter, memory_order_relaxed));

    uint32_t expected = 5;
    bool swapped = atomic_compare_exchange32(&counter,
                                             &expected,
                                             9,
                                             memory_order_acq_rel,
                                             memory_order_relaxed);
    TEST_ASSERT_TRUE(swapped);
    TEST_ASSERT_EQUAL_UINT32(9, atomic_load32(&counter, memory_order_relaxed));

    uint32_t prior = atomic_fetch_and32(&counter, 0xF, memory_order_acq_rel);
    TEST_ASSERT_EQUAL_UINT32(9, prior);
    TEST_ASSERT_EQUAL_UINT32(9 & 0xF, atomic_load32(&counter, memory_order_relaxed));
}

static void test_atomic64_operations(void) {
    atomic64_t wide = { .value = 0 };

    atomic_store64(&wide, 1ULL << 40, memory_order_release);
    TEST_ASSERT_EQUAL_UINT64(1ULL << 40, atomic_load64(&wide, memory_order_acquire));

    uint64_t prev = atomic_fetch_or64(&wide, 0xFF, memory_order_acq_rel);
    TEST_ASSERT_EQUAL_UINT64(1ULL << 40, prev);
    TEST_ASSERT_EQUAL_UINT64((1ULL << 40) | 0xFF, atomic_load64(&wide, memory_order_relaxed));

    uint64_t prior = atomic_fetch_sub64(&wide, 0x10, memory_order_acq_rel);
    TEST_ASSERT_EQUAL_UINT64((1ULL << 40) | 0xFF, prior);
    TEST_ASSERT_EQUAL_UINT64(((1ULL << 40) | 0xFF) - 0x10,
                             atomic_load64(&wide, memory_order_relaxed));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_atomic32_operations);
    RUN_TEST(test_atomic64_operations);
    return UNITY_END();
}
