#include <kernel/tsc.h>
#include <unity.h>

void setUp(void) {
    tsc_override_calibration(1000000000ull, 5ull); // 1 GHz, boot time 5s
}

void tearDown(void) {
}

static void test_tsc_ticks_to_ns(void) {
    TEST_ASSERT_EQUAL_UINT64(500ull, tsc_ticks_to_ns(500ull));
}

static void test_tsc_ns_to_ticks(void) {
    TEST_ASSERT_EQUAL_UINT64(2000ull, tsc_ns_to_ticks(2000ull));
}

static void test_tsc_frequency_query(void) {
    TEST_ASSERT_EQUAL_UINT64(1000000000ull, tsc_frequency_hz());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tsc_ticks_to_ns);
    RUN_TEST(test_tsc_ns_to_ticks);
    RUN_TEST(test_tsc_frequency_query);
    return UNITY_END();
}
