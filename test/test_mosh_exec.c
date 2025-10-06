#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#include <kernel/syscall.h>

#define MOSH_TEST

#define MOSH_CAPTURE_CAPACITY 4096

static char   g_capture[MOSH_CAPTURE_CAPACITY];
static size_t g_capture_length;

static void mosh_test_reset_output(void);
void mosh_test_write_bytes(int fd, const char* data, size_t length);
long mosh_test_syscall0(long number);
long mosh_test_syscall1(long number, long arg1);
long mosh_test_syscall2(long number, long arg1, long arg2);
long mosh_test_syscall3(long number, long arg1, long arg2, long arg3);
void mosh_test_set_env(char** envp);
static char* g_test_envp[] = { "PATH=/bin", NULL };

static long g_mock_fork_result;
static long g_mock_waitpid_result;
static int  g_mock_waitpid_status;
static long g_mock_execve_result;
static int  g_syscall_execve_calls;

#include "../app/mosh/mosh.c"

static void mosh_test_reset_output(void) {
  g_capture_length = 0;
}

void mosh_test_write_bytes(int fd, const char* data, size_t length) {
  (void)fd;
  if(data == NULL || length == 0) {
    return;
  }
  if(length > MOSH_CAPTURE_CAPACITY - g_capture_length) {
    length = MOSH_CAPTURE_CAPACITY - g_capture_length;
  }
  if(length == 0) {
    return;
  }
  memcpy(&g_capture[g_capture_length], data, length);
  g_capture_length += length;
}

long mosh_test_syscall0(long number) {
  TEST_ASSERT_EQUAL_MESSAGE(SYS_FORK, number, "syscall0 expected SYS_FORK");
  return g_mock_fork_result;
}

long mosh_test_syscall1(long number, long arg1) {
  (void)number;
  (void)arg1;
  /* Only hit in the simulated child exit path; never executed in tests. */
  TEST_FAIL_MESSAGE("syscall1 should not be invoked in parent-path tests");
  return 0;
}

long mosh_test_syscall2(long number, long arg1, long arg2) {
  (void)number;
  (void)arg1;
  (void)arg2;
  return 0;
}

long mosh_test_syscall3(long number, long arg1, long arg2, long arg3) {
  (void)arg1;
  (void)arg3;
  if(number == SYS_EXECVE) {
    g_syscall_execve_calls++;
    return g_mock_execve_result;
  }

  if(number == SYS_WAITPID) {
    int* status_ptr = (int*)arg2;
    if(status_ptr != NULL) {
      *status_ptr = g_mock_waitpid_status;
    }
    return g_mock_waitpid_result;
  }

  TEST_FAIL_MESSAGE("Unexpected syscall3 number");
  return -1;
}

static void run_launch_command(const char* line) {
  char buffer[128];
  memset(buffer, 0, sizeof(buffer));
  strncpy(buffer, line, sizeof(buffer) - 1);
  launch_command(buffer);
}

void setUp(void) {
  mosh_test_reset_output();
  g_mock_fork_result = 1234;
  g_mock_waitpid_result = 1234;
  g_mock_waitpid_status = 0;
  g_mock_execve_result = 0;
  g_syscall_execve_calls = 0;
  mosh_test_set_env(g_test_envp);
}

void tearDown(void) {}

static int find_subsequence(const char* haystack, size_t hay_len, const char* needle, size_t needle_len) {
  if(needle_len == 0 || hay_len < needle_len) {
    return -1;
  }
  for(size_t i = 0; i <= hay_len - needle_len; i++) {
    if(memcmp(&haystack[i], needle, needle_len) == 0) {
      return (int)i;
    }
  }
  return -1;
}

void test_launch_command_reports_nonzero_exit_status(void) {
  g_mock_waitpid_status = 7;
  run_launch_command("ls");

  TEST_ASSERT_EQUAL_INT(0, g_syscall_execve_calls);

  const char expected[] = "mosh: process exited with status 7\n";
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      0,
      find_subsequence(g_capture, g_capture_length, expected, sizeof(expected) - 1),
      "Expected non-zero exit status message");
}

void test_launch_command_prints_waitpid_error(void) {
  g_mock_waitpid_result = -5;
  run_launch_command("ls");

  const char expected[] = "mosh: waitpid failed\n";
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      0,
      find_subsequence(g_capture, g_capture_length, expected, sizeof(expected) - 1),
      "Expected waitpid failure message");
}

void test_launch_command_reports_command_not_found(void) {
  g_mock_waitpid_status = 127;
  g_mock_execve_result = -1;
  run_launch_command("doesnotexist");

  const char expected[] = "mosh: command not found: doesnotexist\n";
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      0,
      find_subsequence(g_capture, g_capture_length, expected, sizeof(expected) - 1),
      "Expected command not found message");
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_launch_command_reports_nonzero_exit_status);
  RUN_TEST(test_launch_command_prints_waitpid_error);
  RUN_TEST(test_launch_command_reports_command_not_found);

  return UNITY_END();
}
