#include <kernel/semaphore.h>
#include <kernel/proc.h>
#include <unity.h>

#include <string.h>

extern proc_info_p current;

static ksem_t semaphore;
static proc_info_t fake_proc;

void setUp(void) {
    memset(&semaphore, 0, sizeof(semaphore));
    memset(&fake_proc, 0, sizeof(fake_proc));
    fake_proc.pid = 1;
    fake_proc.state = PROC_STATE_RUNNING;
    current = &fake_proc;
    ksem_init(&semaphore, 0);
}

void tearDown(void) {
    current = NULL;
}

static void test_ksem_trywait_fails_when_empty(void) {
    TEST_ASSERT_FALSE(ksem_trywait(&semaphore));
    TEST_ASSERT_EQUAL_INT64(0, semaphore.count);
}

static void test_ksem_wait_consumes_token(void) {
    semaphore.count = 1;

    ksem_wait(&semaphore);

    TEST_ASSERT_EQUAL_INT64(0, semaphore.count);
}

static void test_ksem_post_then_trywait(void) {
    ksem_post(&semaphore);
    TEST_ASSERT_EQUAL_INT64(1, semaphore.count);

    TEST_ASSERT_TRUE(ksem_trywait(&semaphore));
    TEST_ASSERT_EQUAL_INT64(0, semaphore.count);
}

static void test_ksem_multiple_post_wait_cycles(void) {
    const int tokens = 5;

    for(int i = 0; i < tokens; ++i) {
        ksem_post(&semaphore);
    }

    TEST_ASSERT_EQUAL_INT64(tokens, semaphore.count);

    for(int i = 0; i < tokens; ++i) {
        TEST_ASSERT_TRUE(ksem_trywait(&semaphore));
    }

    TEST_ASSERT_EQUAL_INT64(0, semaphore.count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ksem_trywait_fails_when_empty);
    RUN_TEST(test_ksem_wait_consumes_token);
    RUN_TEST(test_ksem_post_then_trywait);
    RUN_TEST(test_ksem_multiple_post_wait_cycles);
    return UNITY_END();
}
