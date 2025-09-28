#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <unity.h>

static kmutex_t mutex;
static proc_info_t proc_a;
static proc_info_t proc_b;

extern proc_info_p current;

void setUp(void) {
    kmutex_init(&mutex);
    proc_a.pid = 1;
    proc_b.pid = 2;
    current = &proc_a;
}

void tearDown(void) {
    current = NULL;
}

static void test_kmutex_lock_unlock(void) {
    TEST_ASSERT_EQUAL_INT(0, kmutex_lock(&mutex));
    TEST_ASSERT_EQUAL_PTR(current, mutex.owner);
    TEST_ASSERT_EQUAL_INT(0, kmutex_unlock(&mutex));
    TEST_ASSERT_NULL(mutex.owner);
}

static void test_kmutex_trylock_contention(void) {
    TEST_ASSERT_TRUE(kmutex_trylock(&mutex));
    TEST_ASSERT_EQUAL_PTR(&proc_a, mutex.owner);

    current = &proc_b;
    TEST_ASSERT_FALSE(kmutex_trylock(&mutex));

    current = &proc_a;
    kmutex_unlock(&mutex);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_kmutex_lock_unlock);
    RUN_TEST(test_kmutex_trylock_contention);
    return UNITY_END();
}
