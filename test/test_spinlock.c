#include <kernel/spinlock.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void test_spinlock_basic(void) {
    spinlock_t lock;
    spinlock_init(&lock);
    TEST_ASSERT_FALSE(spinlock_is_locked(&lock));

    spinlock_lock(&lock);
    TEST_ASSERT_TRUE(spinlock_is_locked(&lock));

    TEST_ASSERT_FALSE(spinlock_trylock(&lock));

    spinlock_unlock(&lock);
    TEST_ASSERT_FALSE(spinlock_is_locked(&lock));

    TEST_ASSERT_TRUE(spinlock_trylock(&lock));
    TEST_ASSERT_TRUE(spinlock_is_locked(&lock));
    spinlock_unlock(&lock);
    TEST_ASSERT_FALSE(spinlock_is_locked(&lock));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_spinlock_basic);
    return UNITY_END();
}
