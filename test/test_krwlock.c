#include <kernel/rwlock.h>
#include <kernel/proc.h>
#include <unity.h>

#include <string.h>

extern proc_info_p current;

static krwlock_t rwlock;
static proc_info_t reader_proc_a;
static proc_info_t reader_proc_b;
static proc_info_t writer_proc;

void setUp(void) {
    memset(&rwlock, 0, sizeof(rwlock));
    memset(&reader_proc_a, 0, sizeof(reader_proc_a));
    memset(&reader_proc_b, 0, sizeof(reader_proc_b));
    memset(&writer_proc, 0, sizeof(writer_proc));

    reader_proc_a.pid = 1;
    reader_proc_a.state = PROC_STATE_RUNNING;
    reader_proc_b.pid = 3;
    reader_proc_b.state = PROC_STATE_RUNNING;
    writer_proc.pid = 2;
    writer_proc.state = PROC_STATE_RUNNING;

    current = &reader_proc_a;
    krwlock_init(&rwlock);
}

void tearDown(void) {
    current = NULL;
}

static void test_krwlock_rdlock_tracks_reader_count(void) {
    krwlock_rdlock(&rwlock);
    TEST_ASSERT_EQUAL_UINT32(1, rwlock.readers);
    TEST_ASSERT_FALSE(rwlock.writer_active);

    krwlock_rdunlock(&rwlock);
    TEST_ASSERT_EQUAL_UINT32(0, rwlock.readers);
}

static void test_krwlock_allows_multiple_readers(void) {
    krwlock_rdlock(&rwlock);

    current = &reader_proc_b;
    krwlock_rdlock(&rwlock);

    TEST_ASSERT_EQUAL_UINT32(2, rwlock.readers);
    TEST_ASSERT_FALSE(rwlock.writer_active);

    krwlock_rdunlock(&rwlock);
    current = &reader_proc_a;
    krwlock_rdunlock(&rwlock);
    TEST_ASSERT_EQUAL_UINT32(0, rwlock.readers);
}

static void test_krwlock_trywrlock_fails_when_reader_active(void) {
    krwlock_rdlock(&rwlock);

    current = &writer_proc;
    TEST_ASSERT_FALSE(krwlock_trywrlock(&rwlock));

    current = &reader_proc_a;
    krwlock_rdunlock(&rwlock);
}

static void test_krwlock_wrlock_sets_writer_active(void) {
    current = &writer_proc;
    krwlock_wrlock(&rwlock);

    TEST_ASSERT_TRUE(rwlock.writer_active);
    TEST_ASSERT_EQUAL_UINT32(0, rwlock.readers);

    krwlock_wrunlock(&rwlock);
    TEST_ASSERT_FALSE(rwlock.writer_active);
}

static void test_krwlock_tryrdlock_rejects_active_writer(void) {
    current = &writer_proc;
    krwlock_wrlock(&rwlock);

    current = &reader_proc_a;
    TEST_ASSERT_FALSE(krwlock_tryrdlock(&rwlock));

    current = &writer_proc;
    krwlock_wrunlock(&rwlock);
}

static void test_krwlock_wrlock_resets_waiter_count(void) {
    current = &writer_proc;
    krwlock_wrlock(&rwlock);

    TEST_ASSERT_EQUAL_UINT32(0, rwlock.writers_waiting);
    TEST_ASSERT_TRUE(rwlock.writer_active);

    krwlock_wrunlock(&rwlock);
    TEST_ASSERT_FALSE(rwlock.writer_active);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_krwlock_rdlock_tracks_reader_count);
    RUN_TEST(test_krwlock_allows_multiple_readers);
    RUN_TEST(test_krwlock_trywrlock_fails_when_reader_active);
    RUN_TEST(test_krwlock_wrlock_sets_writer_active);
    RUN_TEST(test_krwlock_tryrdlock_rejects_active_writer);
    RUN_TEST(test_krwlock_wrlock_resets_waiter_count);
    return UNITY_END();
}
