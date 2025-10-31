#include <errno.h>
#include <string.h>

#include <unity.h>

#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/syscall.h>

extern proc_info_t kernel_process_info;
extern proc_info_p procs[PROC_MAX];

static proc_info_t init_proc;

static uint64_t dispatch_alarm(unsigned int seconds) {
  syscall_frame_t frame = {
    .rax = SYS_ALARM,
    .rdi = seconds,
  };
  return syscall_dispatch(&frame);
}

void setUp(void) {
  memset(&init_proc, 0, sizeof(init_proc));
  init_proc.pid = 1;
  init_proc.user_mode = true;
  init_proc.quantum_us = 1000;

  proc_signal_state_initialize(&init_proc);

  current = &init_proc;
  procs[0] = &kernel_process_info;
  procs[1] = &init_proc;

  syscall_initialize();
}

void tearDown(void) {
  current = NULL;
}

void test_alarm_arms_timer_and_returns_previous_remaining(void) {
  uint64_t first = dispatch_alarm(1);
  TEST_ASSERT_EQUAL_UINT64(0, first);

  proc_itimer_t* timer = &init_proc.timers[PROC_ITIMER_REAL];
  TEST_ASSERT_TRUE(timer->active);
  TEST_ASSERT_EQUAL_UINT64(0, timer->interval_us);

  uint64_t prev = dispatch_alarm(0);
  TEST_ASSERT_TRUE(prev >= 1);
  TEST_ASSERT_FALSE(init_proc.timers[PROC_ITIMER_REAL].active);
  TEST_ASSERT_EQUAL_UINT64(0, init_proc.timers[PROC_ITIMER_REAL].interval_us);
  TEST_ASSERT_EQUAL_UINT64(0, init_proc.timers[PROC_ITIMER_REAL].expires_us);
}

void test_alarm_replaces_existing_timer_and_returns_remaining_seconds(void) {
  uint64_t first = dispatch_alarm(5);
  TEST_ASSERT_EQUAL_UINT64(0, first);

  uint64_t second = dispatch_alarm(3);
  TEST_ASSERT_TRUE(second >= 4);
  TEST_ASSERT_TRUE(second <= 5);
  TEST_ASSERT_TRUE(init_proc.timers[PROC_ITIMER_REAL].active);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_alarm_arms_timer_and_returns_previous_remaining);
  RUN_TEST(test_alarm_replaces_existing_timer_and_returns_remaining_seconds);
  return UNITY_END();
}
