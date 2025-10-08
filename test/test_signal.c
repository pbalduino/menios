#include <errno.h>
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
  parent.signal_actions[SIGTERM].sa_handler = (sighandler_t)0x1234;
  proc_signal_state_copy(&proc, &parent);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGTERM), proc.signal_blocked);
  TEST_ASSERT_EQUAL_PTR(parent.signal_actions[SIGTERM].sa_handler, proc.signal_actions[SIGTERM].sa_handler);
  TEST_ASSERT_EQUAL_UINT32(parent.signal_actions[SIGTERM].sa_mask, proc.signal_actions[SIGTERM].sa_mask);
  TEST_ASSERT_FALSE(proc_signal_pending(&proc));
}

void test_signal_configure_action_sets_handler(void) {
  struct sigaction action = {
    .sa_handler = (sighandler_t)0x1234,
    .sa_mask = sigbit(SIGINT),
    .sa_flags = 0
  };

  struct sigaction previous = {0};
  int rc = proc_signal_configure_action(&proc, SIGTERM, &action, &previous);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_PTR(SIG_DFL, previous.sa_handler);
  TEST_ASSERT_EQUAL_PTR(action.sa_handler, proc.signal_actions[SIGTERM].sa_handler);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGINT), proc.signal_actions[SIGTERM].sa_mask);
}

void test_signal_configure_action_rejects_sigkill_override(void) {
  struct sigaction action = {
    .sa_handler = (sighandler_t)0x1234,
    .sa_mask = 0,
    .sa_flags = 0
  };

  int rc = proc_signal_configure_action(&proc, SIGKILL, &action, NULL);
  TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
}

void test_signal_modify_mask_blocks_and_unblocks(void) {
  uint32_t previous = 0;
  int rc = proc_signal_modify_mask(&proc, SIG_BLOCK, sigbit(SIGINT), &previous);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_UINT32(0, previous);
  TEST_ASSERT_TRUE(proc.signal_blocked & sigbit(SIGINT));

  rc = proc_signal_modify_mask(&proc, SIG_UNBLOCK, sigbit(SIGINT), &previous);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGINT), previous);
  TEST_ASSERT_EQUAL_UINT32(0, proc.signal_blocked);
}

void test_signal_send_validates_signo(void) {
  int rc = proc_signal_send(&proc, 0);
  TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_signal_enqueue_and_dequeue);
  RUN_TEST(test_blocked_signal_stays_pending);
  RUN_TEST(test_signal_state_copy_inherits_handlers);
  RUN_TEST(test_signal_configure_action_sets_handler);
  RUN_TEST(test_signal_configure_action_rejects_sigkill_override);
  RUN_TEST(test_signal_modify_mask_blocks_and_unblocks);
  RUN_TEST(test_signal_send_validates_signo);
  return UNITY_END();
}
