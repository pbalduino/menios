#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

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
static long g_waitpid_plan_results[8];
static int  g_waitpid_plan_status[8];
static size_t g_waitpid_plan_length;
static size_t g_waitpid_plan_index;
static int  g_syscall_kill_calls;
static pid_t g_last_kill_pid;
static int  g_last_kill_signo;

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
  if(number == SYS_FORK) {
    return g_mock_fork_result;
  }
  if(number == SYS_STDIN_POLL) {
    return -1;
  }
  TEST_FAIL_MESSAGE("Unexpected syscall0 number");
  return -1;
}

long mosh_test_syscall1(long number, long arg1) {
  (void)number;
  (void)arg1;
  /* Only hit in the simulated child exit path; never executed in tests. */
  TEST_FAIL_MESSAGE("syscall1 should not be invoked in parent-path tests");
  return 0;
}

long mosh_test_syscall2(long number, long arg1, long arg2) {
  if(number == SYS_KILL) {
    g_syscall_kill_calls++;
    g_last_kill_pid = (pid_t)arg1;
    g_last_kill_signo = (int)arg2;
    return 0;
  }

  if(number == SYS_PROC_KILL) {
    return 0;
  }

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
    long result = g_mock_waitpid_result;
    int status = g_mock_waitpid_status;
    if(g_waitpid_plan_length > 0) {
      size_t idx = g_waitpid_plan_index;
      if(idx >= g_waitpid_plan_length) {
        idx = g_waitpid_plan_length - 1;
      }
      result = g_waitpid_plan_results[idx];
      status = g_waitpid_plan_status[idx];
      if(g_waitpid_plan_index + 1 < g_waitpid_plan_length) {
        g_waitpid_plan_index++;
      }
    }
    if(status_ptr != NULL) {
      *status_ptr = status;
    }
    return result;
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
  g_waitpid_plan_length = 0;
  g_waitpid_plan_index = 0;
  g_syscall_kill_calls = 0;
  g_last_kill_pid = -1;
  g_last_kill_signo = 0;
  mosh_test_set_env(g_test_envp);
  mosh_test_reset_sigint();
  shell_install_signal_handlers();
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

void test_wait_for_children_sends_sigint_to_children(void) {
  command_segment_t segments[1];
  init_segment(&segments[0]);
  segments[0].argc = 1;
  segments[0].argv[0] = "ls";
  segments[0].argv[1] = NULL;

  long pids[1] = { 1234 };
  int statuses[1] = { 0 };

  g_waitpid_plan_length = 2;
  g_waitpid_plan_index = 0;
  g_waitpid_plan_results[0] = 0;
  g_waitpid_plan_status[0] = 0;
  g_waitpid_plan_results[1] = 1234;
  g_waitpid_plan_status[1] = 130;

  mosh_test_trigger_sigint();

  int status = wait_for_children(segments, 1, pids, statuses);

  TEST_ASSERT_EQUAL_INT(130, status);
  TEST_ASSERT_EQUAL_INT(1, g_syscall_kill_calls);
  TEST_ASSERT_EQUAL_INT(1234, g_last_kill_pid);
  TEST_ASSERT_EQUAL_INT(SIGINT, g_last_kill_signo);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_launch_command_reports_nonzero_exit_status);
  RUN_TEST(test_launch_command_prints_waitpid_error);
  RUN_TEST(test_launch_command_reports_command_not_found);
  RUN_TEST(test_wait_for_children_sends_sigint_to_children);

  return UNITY_END();
}
