#include <errno.h>
#include <string.h>

#include <unity.h>

#include <kernel/signal.h>
#include <kernel/proc.h>
#include <menios/signal_frame.h>
#include <sys/wait.h>

static proc_info_t proc;
static proc_info_t parent;

void setUp(void) {
  memset(&proc, 0, sizeof(proc));
  memset(&parent, 0, sizeof(parent));
  proc_signal_state_init(&proc);
  proc_signal_state_init(&parent);
  proc.user_mode = true;
  parent.user_mode = true;
  proc.quantum_us = 1000;
  parent.quantum_us = 1000;
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

void test_signal_configure_action_rejects_sigstop_override(void) {
  struct sigaction action = {
    .sa_handler = (sighandler_t)0x1234,
    .sa_mask = 0,
    .sa_flags = 0
  };

  int rc = proc_signal_configure_action(&proc, SIGSTOP, &action, NULL);
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

  rc = proc_signal_modify_mask(&proc, SIG_BLOCK, sigbit(SIGSTOP), &previous);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_UINT32(0, proc.signal_blocked & sigbit(SIGSTOP));
}

void test_signal_send_validates_signo(void) {
  int rc = proc_signal_send(&proc, 0);
  TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
}

void test_signal_handle_pending_invokes_handler(void) {
#ifdef MENIOS_HOST_TEST
  TEST_IGNORE_MESSAGE("signal frame verification requires kernel runtime");
  return;
#else
  cpu_state_t frame;
  memset(&frame, 0, sizeof(frame));
  const size_t storage_size = sizeof(menios_signal_frame_t) + 512;
  uint8_t stack_storage[storage_size];
  memset(stack_storage, 0, sizeof(stack_storage));
  uintptr_t base = (uintptr_t)stack_storage;
  uintptr_t top = base + storage_size;
  uintptr_t aligned_top = top & ~((uintptr_t)0xF);
  frame.rsp = (uint64_t)aligned_top;
  frame.rip = 0xCAFEBABE;

  proc_signal_enqueue(&proc, SIGTERM);
  proc.signal_actions[SIGTERM].sa_handler = (sighandler_t)0xDEADBEEF;
  proc.signal_actions[SIGTERM].sa_restorer = (sigrestorer_t)0xFEEDFACE;

  proc_signal_delivery_t result = proc_signal_handle_pending(&proc, &frame);

  uintptr_t restorer_slot = (uintptr_t)frame.rsp;
  uintptr_t frame_base = restorer_slot + sizeof(uint64_t);

  TEST_ASSERT_EQUAL_INT(PROC_SIGNAL_DELIVERY_HANDLED, result);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)proc.signal_actions[SIGTERM].sa_handler, frame.rip);
  TEST_ASSERT_EQUAL_UINT64(SIGTERM, frame.rdi);
  TEST_ASSERT_TRUE(restorer_slot >= base);
  TEST_ASSERT_TRUE((restorer_slot + sizeof(uint64_t)) <= base + storage_size);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)proc.signal_actions[SIGTERM].sa_restorer,
                           *((uint64_t*)restorer_slot));

  menios_signal_frame_t* saved_frame = (menios_signal_frame_t*)frame_base;
  TEST_ASSERT_TRUE(frame_base >= base);
  TEST_ASSERT_TRUE((frame_base + sizeof(menios_signal_frame_t)) <= base + storage_size);
  TEST_ASSERT_EQUAL_UINT64(0xCAFEBABE, saved_frame->context.rip);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)(-EINTR), saved_frame->context.rax);
  TEST_ASSERT_EQUAL_INT(SIGTERM, saved_frame->signo);
#endif
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
  TEST_ASSERT_EQUAL_INT(SIGTERM & 0x7f, proc.exit_code);
}

void test_signal_handle_pending_default_stops_process(void) {
  cpu_state_t frame;
  memset(&frame, 0, sizeof(frame));
  current = &proc;
  proc.state = PROC_STATE_RUNNING;
  proc.quantum_us = 1000;

  proc_signal_enqueue(&proc, SIGSTOP);

  proc_signal_delivery_t result = proc_signal_handle_pending(&proc, &frame);

  TEST_ASSERT_EQUAL_INT(PROC_SIGNAL_DELIVERY_STOPPED, result);
  TEST_ASSERT_EQUAL_INT(PROC_STATE_STOPPED, proc.state);
  TEST_ASSERT_TRUE(proc.stop_status_pending);
  TEST_ASSERT_EQUAL_INT(((SIGSTOP & 0x7f) << 8) | 0x7f, proc.stop_status);
}

void test_signal_handle_pending_default_continues_process(void) {
  cpu_state_t frame;
  memset(&frame, 0, sizeof(frame));
  current = &proc;
  proc.state = PROC_STATE_STOPPED;
  proc.quantum_us = 1000;
  proc.stop_status_pending = true;
  proc.stopped = true;

  proc_signal_enqueue(&proc, SIGCONT);

  proc_signal_delivery_t result = proc_signal_handle_pending(&proc, &frame);

  TEST_ASSERT_EQUAL_INT(PROC_SIGNAL_DELIVERY_HANDLED, result);
  TEST_ASSERT_FALSE(proc.stopped);
  TEST_ASSERT_EQUAL_INT(PROC_STATE_READY, proc.state);
  TEST_ASSERT_FALSE(proc.stop_status_pending);
  TEST_ASSERT_TRUE(proc.continued_pending);
  TEST_ASSERT_EQUAL_INT(0xffff, proc.continue_status);
}

void test_proc_waitpid_reports_stop_status(void) {
  proc_info_t child;
  memset(&child, 0, sizeof(child));
  proc_signal_state_init(&child);
  child.pid = 1234;
  child.state = PROC_STATE_STOPPED;
  child.stop_status = ((SIGTSTP & 0x7f) << 8) | 0x7f;
  child.stop_status_pending = true;
  child.quantum_us = 1000;

  parent.first_child = &child;
  parent.children_count = 1;

  int status = 0;
  int result = proc_waitpid(&parent, child.pid, WUNTRACED, &status);

  TEST_ASSERT_EQUAL_INT(child.pid, result);
  TEST_ASSERT_EQUAL_INT(child.stop_status, status);
  TEST_ASSERT_FALSE(child.stop_status_pending);
}

void test_proc_waitpid_reports_continued_status(void) {
  proc_info_t child;
  memset(&child, 0, sizeof(child));
  proc_signal_state_init(&child);
  child.pid = 4321;
  child.state = PROC_STATE_READY;
  child.continue_status = 0xffff;
  child.continued_pending = true;
  child.quantum_us = 1000;

  parent.first_child = &child;
  parent.children_count = 1;

  int status = 0;
  int result = proc_waitpid(&parent, child.pid, WCONTINUED, &status);

  TEST_ASSERT_EQUAL_INT(child.pid, result);
  TEST_ASSERT_EQUAL_INT(0xffff, status);
  TEST_ASSERT_FALSE(child.continued_pending);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_signal_enqueue_and_dequeue);
  RUN_TEST(test_blocked_signal_stays_pending);
  RUN_TEST(test_signal_state_copy_inherits_handlers);
  RUN_TEST(test_signal_configure_action_sets_handler);
  RUN_TEST(test_signal_configure_action_rejects_sigkill_override);
  RUN_TEST(test_signal_configure_action_rejects_sigstop_override);
  RUN_TEST(test_signal_modify_mask_blocks_and_unblocks);
  RUN_TEST(test_signal_send_validates_signo);
  RUN_TEST(test_signal_handle_pending_invokes_handler);
  RUN_TEST(test_signal_handle_pending_honors_block_mask);
  RUN_TEST(test_signal_handle_pending_default_terminates);
  RUN_TEST(test_signal_handle_pending_default_stops_process);
  RUN_TEST(test_signal_handle_pending_default_continues_process);
  RUN_TEST(test_proc_waitpid_reports_stop_status);
  RUN_TEST(test_proc_waitpid_reports_continued_status);
  return UNITY_END();
}
