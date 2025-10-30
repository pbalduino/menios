#include "unity.h"

#include <errno.h>

#define SHUTDOWN_TEST 1
#define SHUTDOWN_REQUEST_POWEROFF_DEFINED 1

static long shutdown_syscall_result = 0;
static int shutdown_syscall_calls = 0;

long shutdown_request_poweroff(void) {
  shutdown_syscall_calls++;
  return shutdown_syscall_result;
}

#include "../app/shutdown/shutdown.c"

void setUp(void) {
  shutdown_syscall_result = 0;
  shutdown_syscall_calls = 0;
  errno = 0;
}

void tearDown(void) {}

void test_shutdown_command_invokes_syscall(void) {
  char* argv[] = { "shutdown", NULL };
  int rc = shutdown_main(1, argv);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_INT(1, shutdown_syscall_calls);
}

void test_shutdown_command_propagates_errors(void) {
  shutdown_syscall_result = -EIO;
  char* argv[] = { "shutdown", NULL };
  int rc = shutdown_main(1, argv);
  TEST_ASSERT_EQUAL_INT(1, rc);
  TEST_ASSERT_EQUAL_INT(1, shutdown_syscall_calls);
  TEST_ASSERT_EQUAL_INT(EIO, errno);
}

void test_shutdown_command_handles_help(void) {
  char* argv[] = { "shutdown", "--help", NULL };
  int rc = shutdown_main(2, argv);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_INT(0, shutdown_syscall_calls);
}

void test_shutdown_command_rejects_extra_args(void) {
  char* argv[] = { "shutdown", "--now", NULL };
  int rc = shutdown_main(2, argv);
  TEST_ASSERT_EQUAL_INT(1, rc);
  TEST_ASSERT_EQUAL_INT(0, shutdown_syscall_calls);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_shutdown_command_invokes_syscall);
  RUN_TEST(test_shutdown_command_propagates_errors);
  RUN_TEST(test_shutdown_command_handles_help);
  RUN_TEST(test_shutdown_command_rejects_extra_args);
  return UNITY_END();
}
