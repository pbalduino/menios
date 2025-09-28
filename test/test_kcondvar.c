#include <kernel/condvar.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <unity.h>

#include <string.h>

extern proc_info_p current;
extern void (*test_proc_request_yield_hook)(void);

static kmutex_t test_mutex;
static kcondvar_t test_condvar;
static proc_info_t waiter;

static void signal_hook(void) {
    kcondvar_signal(&test_condvar);
    test_proc_request_yield_hook = NULL;
}

void setUp(void) {
    kmutex_init(&test_mutex);
    kcondvar_init(&test_condvar);
    memset(&waiter, 0, sizeof(waiter));
    waiter.pid = 1;
    waiter.state = PROC_STATE_RUNNING;
    waiter.quantum_us = 1000;
    current = &waiter;
}

void tearDown(void) {
    current = NULL;
    test_proc_request_yield_hook = NULL;
}

static void test_condvar_wait_and_signal(void) {
    TEST_ASSERT_TRUE(kmutex_trylock(&test_mutex));
    TEST_ASSERT_EQUAL_PTR(current, test_mutex.owner);

    test_proc_request_yield_hook = signal_hook;

    kcondvar_wait(&test_condvar, &test_mutex);

    TEST_ASSERT_EQUAL_PTR(current, test_mutex.owner);
    TEST_ASSERT_NULL(test_condvar.waiters_head);
    TEST_ASSERT_NULL(test_condvar.waiters_tail);
    TEST_ASSERT_NOT_EQUAL(PROC_STATE_WAITING, waiter.state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_condvar_wait_and_signal);
    return UNITY_END();
}
