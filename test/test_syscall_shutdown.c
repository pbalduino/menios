#include "unity.h"

#include <errno.h>
#include <string.h>

#include <kernel/proc.h>
#include <kernel/syscall.h>

extern proc_info_p current;
extern int test_stub_acpi_shutdown_calls;
extern int test_stub_acpi_shutdown_result;

static proc_info_t proc_state;
static cpu_state_t cpu_state;

static uint64_t dispatch_syscall(uint64_t number) {
  syscall_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.rax = number;
  syscall_dispatch(&frame);
  return frame.rax;
}

void setUp(void) {
  memset(&proc_state, 0, sizeof(proc_state));
  proc_state.cpu_state = &cpu_state;
  proc_state.user_mode = true;
  current = &proc_state;
  test_stub_acpi_shutdown_calls = 0;
  test_stub_acpi_shutdown_result = 0;
  syscall_init();
}

void tearDown(void) {
  current = NULL;
}

void test_syscall_shutdown_invokes_acpi_shutdown(void) {
  uint64_t rc = dispatch_syscall(SYS_SHUTDOWN);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_INT(1, test_stub_acpi_shutdown_calls);
}

void test_syscall_shutdown_propagates_errors(void) {
  test_stub_acpi_shutdown_result = -EIO;
  uint64_t rc = dispatch_syscall(SYS_SHUTDOWN);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)-EIO, rc);
  TEST_ASSERT_EQUAL_INT(1, test_stub_acpi_shutdown_calls);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_syscall_shutdown_invokes_acpi_shutdown);
  RUN_TEST(test_syscall_shutdown_propagates_errors);
  return UNITY_END();
}
