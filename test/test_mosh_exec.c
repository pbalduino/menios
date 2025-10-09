#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

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
static size_t g_waitpid_plan_consumed;

#include "../app/mosh/mosh.c"

static void mosh_test_reset_output(void) {
  g_capture_length = 0;
}

static bool capture_contains(const char* needle) {
  if(needle == NULL) {
    return false;
  }
  size_t needle_len = strlen(needle);
  if(needle_len == 0 || g_capture_length < needle_len) {
    return false;
  }
  for(size_t i = 0; i <= g_capture_length - needle_len; i++) {
    if(memcmp(&g_capture[i], needle, needle_len) == 0) {
      return true;
    }
  }
  return false;
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
      g_waitpid_plan_consumed++;
    }
    if(status_ptr != NULL) {
      *status_ptr = status;
    }
    return result;
  }

  TEST_FAIL_MESSAGE("Unexpected syscall3 number");
  return -1;
}

static int run_launch_command(const char* line) {
  char buffer[128];
  memset(buffer, 0, sizeof(buffer));
  strncpy(buffer, line, sizeof(buffer) - 1);
  return launch_command(buffer);
}

static job_t* find_active_job(void) {
  for(size_t i = 0; i < MOSH_MAX_JOBS; i++) {
    if(jobs[i].in_use) {
      return &jobs[i];
    }
  }
  return NULL;
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
  g_waitpid_plan_consumed = 0;
  g_test_envp[0] = "PATH=/bin";
  g_test_envp[1] = NULL;
  mosh_test_set_env(g_test_envp);
  mosh_test_reset_sigint();
  shell_install_signal_handlers();
  memset(jobs, 0, sizeof(jobs));
  next_job_id = 1;
  current_job = NULL;
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
  g_mock_waitpid_status = (7 << 8);
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
  g_mock_waitpid_status = (127 << 8);
  g_mock_execve_result = -1;
  run_launch_command("doesnotexist");

  const char expected[] = "mosh: command not found: doesnotexist";
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      0,
      find_subsequence(g_capture, g_capture_length, expected, sizeof(expected) - 1),
      "Expected command not found message");
}

void test_launch_command_or_executes_second_segment(void) {
  g_waitpid_plan_length = 2;
  g_waitpid_plan_results[0] = 1234;
  g_waitpid_plan_status[0] = 1;
  g_waitpid_plan_results[1] = 1234;
  g_waitpid_plan_status[1] = 0;

  int status = run_launch_command("cmd1 || cmd2");

  TEST_ASSERT_EQUAL_INT(0, status);
  TEST_ASSERT_TRUE(g_waitpid_plan_consumed >= 2);
}

void test_launch_command_or_short_circuits_on_success(void) {
  g_waitpid_plan_length = 2;
  g_waitpid_plan_results[0] = 1234;
  g_waitpid_plan_status[0] = 0;
  g_waitpid_plan_results[1] = 1234;
  g_waitpid_plan_status[1] = 77;

  int status = run_launch_command("cmd1 || cmd2");

  TEST_ASSERT_EQUAL_INT(0, status);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)g_waitpid_plan_consumed);
}

void test_wait_for_children_sends_sigint_to_children(void) {
  g_waitpid_plan_length = 2;
  g_waitpid_plan_index = 0;
  g_waitpid_plan_results[0] = 0;
  g_waitpid_plan_status[0] = 0;
  g_waitpid_plan_results[1] = 1234;
  g_waitpid_plan_status[1] = 130;

  mosh_test_trigger_sigint();

  int status = run_launch_command("ls");

  TEST_ASSERT_EQUAL_INT(130, status);
  TEST_ASSERT_EQUAL_INT(1, g_syscall_kill_calls);
  TEST_ASSERT_EQUAL_INT(1234, g_last_kill_pid);
  TEST_ASSERT_EQUAL_INT(SIGINT, g_last_kill_signo);
  job_t* remaining = find_active_job();
  if(remaining != NULL) {
    TEST_ASSERT_EQUAL_INT(JOB_STATE_DONE, remaining->state);
    job_release(remaining);
  }
}

void test_launch_command_background_creates_job(void) {
  g_mock_waitpid_result = 0;
  g_mock_waitpid_status = 0;

  int status = run_launch_command("sleep &");
  TEST_ASSERT_EQUAL_INT(0, status);

  job_t* job = find_active_job();
  TEST_ASSERT_NOT_NULL(job);
  TEST_ASSERT_EQUAL_INT(JOB_STATE_RUNNING, job->state);
  TEST_ASSERT_TRUE(job->background);

  const char expected[] = "[1] 1234\n";
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      0,
      find_subsequence(g_capture, g_capture_length, expected, sizeof(expected) - 1),
      "Expected background job notification");

  job_release(job);
}

void test_fg_resumes_stopped_job(void) {
  g_mock_waitpid_result = 0;
  g_mock_waitpid_status = 0;
  run_launch_command("sleep &");

  job_t* job = find_active_job();
  TEST_ASSERT_NOT_NULL(job);
  job->state = JOB_STATE_STOPPED;
  job->background = false;
  job->foreground = false;
  job->segment_count = 1;
  job->pids[0] = 1234;

  g_waitpid_plan_length = 2;
  g_waitpid_plan_index = 0;
  g_waitpid_plan_results[0] = 0;
  g_waitpid_plan_status[0] = 0;
  g_waitpid_plan_results[1] = 1234;
  g_waitpid_plan_status[1] = (2 << 8);

  int status = run_launch_command("fg");

  TEST_ASSERT_EQUAL_INT((2 << 8), status);
  TEST_ASSERT_TRUE(WIFEXITED(status));
  TEST_ASSERT_EQUAL_INT(2, WEXITSTATUS(status));
  TEST_ASSERT_TRUE(g_waitpid_plan_consumed >= 2);

  job_t* remaining = find_active_job();
  TEST_ASSERT_TRUE(remaining == NULL || remaining->state == JOB_STATE_DONE);
  if(remaining != NULL) {
    job_release(remaining);
  }

  const char expected[] = "sleep";
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
      0,
      find_subsequence(g_capture, g_capture_length, expected, (size_t)strlen(expected)),
      "Expected fg to print job command");
}

void test_if_executes_then_branch(void) {
  mosh_test_reset_output();
  run_launch_command("if __test_success { echo then-branch } else { echo else-branch }");
  TEST_ASSERT_TRUE_MESSAGE(g_capture_length > 0, "no output captured");
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("then-branch"), "expected then branch to run");
  TEST_ASSERT_FALSE_MESSAGE(capture_contains("else-branch"), "did not expect else branch");
}

void test_if_executes_else_branch(void) {
  mosh_test_reset_output();
  run_launch_command("if __test_failure { echo then-branch } else { echo else-branch }");
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("else-branch"), "expected else branch to run");
}

#if 0
void test_while_loops_until_counter_limit(void) {
  mosh_test_reset_output();
  run_launch_command("__test_set_counter 3");
  run_launch_command("while __test_counter_lt { echo loop }");
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("loop\nloop\nloop"), "expected loop to run three times");
}
#endif

void test_for_iterates_over_expanded_list(void) {
  mosh_test_reset_output();
  run_launch_command("set items=[alpha, beta]");
  run_launch_command("for item in $items { echo $item }");
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("alpha"), "expected alpha");
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("beta"), "expected beta");
}

void test_function_invocation_with_arguments(void) {
  mosh_test_reset_output();
  run_launch_command("function greet() { echo Hello $1; }");
  run_launch_command("greet world");
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("Hello world"), "expected function output");
}

void test_unset_removes_shell_variable(void) {
  run_launch_command("set FOO=bar");
  const char* before = shell_var_get("FOO");
  TEST_ASSERT_NOT_NULL(before);
  TEST_ASSERT_EQUAL_STRING("bar", before);

  int status = run_launch_command("unset FOO");
  TEST_ASSERT_EQUAL_INT(0, status);
  TEST_ASSERT_NULL(shell_var_get("FOO"));
}

void test_unset_removes_environment_entry(void) {
  TEST_ASSERT_NOT_NULL(env_get("PATH"));

  int status = run_launch_command("unset PATH");
  TEST_ASSERT_EQUAL_INT(0, status);
  TEST_ASSERT_NULL(env_get("PATH"));
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_if_executes_then_branch);
  RUN_TEST(test_if_executes_else_branch);
  RUN_TEST(test_unset_removes_shell_variable);
  RUN_TEST(test_unset_removes_environment_entry);

  return UNITY_END();
}
