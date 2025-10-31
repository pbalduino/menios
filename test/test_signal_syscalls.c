#include <errno.h>
#include <string.h>

#include <unity.h>

#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/syscall.h>

extern proc_info_t kernel_process_info;
extern proc_info_p procs[PROC_MAX];

static proc_info_t init_proc;
static proc_info_t child_proc;

void setUp(void) {
  memset(&init_proc, 0, sizeof(init_proc));
  memset(&child_proc, 0, sizeof(child_proc));

  init_proc.pid = 1;
  child_proc.pid = 42;

  proc_signal_state_initialize(&init_proc);
  proc_signal_state_initialize(&child_proc);

  procs[0] = &kernel_process_info;
  procs[1] = &init_proc;
  procs[2] = &child_proc;

  current = &init_proc;

  syscall_initialize();
}

void tearDown(void) {
  current = NULL;
}

static uint64_t dispatch(uint64_t number, uint64_t rdi, uint64_t rsi, uint64_t rdx) {
  syscall_frame_t frame = {
    .rax = number,
    .rdi = rdi,
    .rsi = rsi,
    .rdx = rdx,
  };

  return syscall_dispatch(&frame);
}

void test_kill_enqueues_signal(void) {
  uint64_t rc = dispatch(SYS_KILL, (uint64_t)child_proc.pid, SIGTERM, 0);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_TRUE(child_proc.signal_pending & sigbit(SIGTERM));
}

void test_kill_unknown_pid_returns_esrch(void) {
  uint64_t rc = dispatch(SYS_KILL, 999, SIGTERM, 0);
  TEST_ASSERT_EQUAL_INT64(-ESRCH, (int64_t)rc);
}

void test_sigaction_sets_handler_and_returns_previous(void) {
  struct sigaction act = {
    .sa_handler = (sighandler_t)0xDEADBEEF,
    .sa_mask = sigbit(SIGINT),
    .sa_flags = 0
  };
  struct sigaction previous = {0};

  uint64_t rc = dispatch(SYS_SIGACTION, SIGTERM, (uint64_t)&act, (uint64_t)&previous);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_PTR(act.sa_handler, init_proc.signal_actions[SIGTERM].sa_handler);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGINT), init_proc.signal_actions[SIGTERM].sa_mask);
  TEST_ASSERT_EQUAL_PTR(SIG_DFL, previous.sa_handler);
}

void test_sigprocmask_updates_mask(void) {
  init_proc.signal_blocked = sigbit(SIGINT);
  sigset_t new_mask = sigbit(SIGTERM);
  sigset_t old_mask = 0;

  uint64_t rc = dispatch(SYS_SIGPROCMASK, SIG_SETMASK, (uint64_t)&new_mask, (uint64_t)&old_mask);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGTERM), init_proc.signal_blocked);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGINT), old_mask);
}

void test_sigprocmask_with_null_set_only_reports_old(void) {
  init_proc.signal_blocked = sigbit(SIGTERM);
  sigset_t old_mask = 0;

  uint64_t rc = dispatch(SYS_SIGPROCMASK, SIG_BLOCK, 0, (uint64_t)&old_mask);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGTERM), old_mask);
  TEST_ASSERT_EQUAL_UINT32(sigbit(SIGTERM), init_proc.signal_blocked);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_kill_enqueues_signal);
  RUN_TEST(test_kill_unknown_pid_returns_esrch);
  RUN_TEST(test_sigaction_sets_handler_and_returns_previous);
  RUN_TEST(test_sigprocmask_updates_mask);
  RUN_TEST(test_sigprocmask_with_null_set_only_reports_old);
  return UNITY_END();
}
