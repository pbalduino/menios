#include <kernel/proc.h>
#include <kernel/syscall.h>
#include <menios/syscall.h>
#include <unity.h>

#include <stdint.h>
#include <string.h>

extern proc_info_p current;
extern proc_info_t kernel_process_info;

static proc_info_t test_proc;
static cpu_state_t test_cpu_state;
static syscall_frame_t frame;
static int init_done = 0;

void setUp(void) {
  if(!init_done) {
    syscall_init();
    init_done = 1;
  }

  memset(&test_proc, 0, sizeof(test_proc));
  memset(&test_cpu_state, 0, sizeof(test_cpu_state));
  memset(&frame, 0, sizeof(frame));

  test_proc.pid = 42;
  test_proc.cpu_state = &test_cpu_state;
  current = &test_proc;
}

void tearDown(void) {
  current = &kernel_process_info;
}

void test_syscall_dispatch_preserves_return_value_in_cpu_state(void) {
  frame.rax = SYS_GETPAGESIZE;

  uint64_t result = syscall_dispatch(&frame);

  TEST_ASSERT_EQUAL_UINT64(result, frame.rax);
  TEST_ASSERT_NOT_NULL(current->cpu_state);
  TEST_ASSERT_EQUAL_UINT64(result, current->cpu_state->rax);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_syscall_dispatch_preserves_return_value_in_cpu_state);
  return UNITY_END();
}
