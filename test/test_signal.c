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
  proc.user_mode = true;
  parent.user_mode = true;
}

void tearDown(void) {
  current = NULL;
}

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

void test_signal_handle_pending_invokes_handler(void) {
  cpu_state_t frame;
  memset(&frame, 0, sizeof(frame));
  uint64_t stack_buf[16] = {0};
  frame.rsp = (uint64_t)(stack_buf + 8);
  frame.rip = 0xCAFEBABE;

  proc_signal_enqueue(&proc, SIGTERM);
  proc.signal_actions[SIGTERM].sa_handler = (sighandler_t)0xDEADBEEF;

  proc_signal_delivery_t result = proc_signal_handle_pending(&proc, &frame);

  TEST_ASSERT_EQUAL_INT(PROC_SIGNAL_DELIVERY_HANDLED, result);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)proc.signal_actions[SIGTERM].sa_handler, frame.rip);
  TEST_ASSERT_EQUAL_UINT64(SIGTERM, frame.rdi);
  TEST_ASSERT_EQUAL_UINT64(0xCAFEBABE, stack_buf[7]);
  TEST_ASSERT_EQUAL_PTR(stack_buf + 7, (uint64_t*)frame.rsp);
}

void test_signal_handle_pending_honors_block_mask(void) {
  cpu_state_t frame;
  memset(&frame, 0, sizeof(frame));

  proc_signal_enqueue(&proc, SIGINT);
  proc_signal_set_blocked(&proc, sigbit(SIGINT));

  proc_signal_delivery_t result = proc_signal_handle_pending(&proc, &frame);

  TEST_ASSERT_EQUAL_INT(PROC_SIGNAL_DELIVERY_NONE, result);
  TEST_ASSERT_TRUE(proc_signal_pending(&proc));
}

void test_signal_handle_pending_default_terminates(void) {
  cpu_state_t frame;
  memset(&frame, 0, sizeof(frame));
  current = &proc;
  proc.state = PROC_STATE_RUNNING;

  proc_signal_enqueue(&proc, SIGTERM);

  proc_signal_delivery_t result = proc_signal_handle_pending(&proc, &frame);

  TEST_ASSERT_EQUAL_INT(PROC_SIGNAL_DELIVERY_TERMINATED, result);
  TEST_ASSERT_EQUAL_INT(PROC_STATE_ZOMBIE, proc.state);
  TEST_ASSERT_EQUAL_INT(128 + SIGTERM, proc.exit_code);
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
  RUN_TEST(test_signal_handle_pending_invokes_handler);
  RUN_TEST(test_signal_handle_pending_honors_block_mask);
  RUN_TEST(test_signal_handle_pending_default_terminates);
  return UNITY_END();
}
