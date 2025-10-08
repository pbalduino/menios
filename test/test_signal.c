#include <string.h>

#include <unity.h>

#include <kernel/signal.h>
#include <kernel/proc.h>

static proc_info_t proc;
static proc_info_t parent;

void setUp(void) {
  memset(&proc, 0, sizeof(proc));
  memset(&parent, 0, sizeof(parent));
  proc_signal_state_init(&proc);
  proc_signal_state_init(&parent);
}

void tearDown(void) {}

void test_signal_enqueue_and_dequeue(void) {
  TEST_ASSERT_FALSE(proc_signal_pending(&proc));
  proc_signal_enqueue(&proc, SIGINT);
  TEST_ASSERT_TRUE(proc_signal_pending(&proc));
  int signo = proc_signal_dequeue(&proc);
  TEST_ASSERT_EQUAL_INT(SIGINT, signo);
  TEST_ASSERT_FALSE(proc_signal_pending(&proc));
}

void test_blocked_signal_stays_pending(void) {
  proc_signal_enqueue(&proc, SIGINT);
  proc_signal_set_blocked(&proc, sigbit(SIGINT));
  TEST_ASSERT_TRUE(proc_signal_pending(&proc));
  TEST_ASSERT_EQUAL_INT(-1, proc_signal_dequeue(&proc));
  proc_signal_set_blocked(&proc, 0);
  TEST_ASSERT_EQUAL_INT(SIGINT, proc_signal_dequeue(&proc));
}

void test_signal_state_copy_inherits_handlers(void) {
  parent.signal_blocked = sigbit(SIGTERM);
  parent.signal_handlers[SIGTERM] = (sighandler_t)0x1234;
  proc_signal_state_copy(&proc, &parent);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGTERM), proc.signal_blocked);
  TEST_ASSERT_EQUAL_PTR(parent.signal_handlers[SIGTERM], proc.signal_handlers[SIGTERM]);
  TEST_ASSERT_FALSE(proc_signal_pending(&proc));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_signal_enqueue_and_dequeue);
  RUN_TEST(test_blocked_signal_stays_pending);
  RUN_TEST(test_signal_state_copy_inherits_handlers);
  return UNITY_END();
}
