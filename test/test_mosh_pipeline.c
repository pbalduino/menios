#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#include <kernel/syscall.h>

#define MOSH_TEST

#define MOSH_CAPTURE_CAPACITY 4096

static char   g_capture[MOSH_CAPTURE_CAPACITY];
static size_t g_capture_length;
static long   g_fork_calls;
static int    g_waitpid_calls;

long mosh_test_syscall0(long number);
long mosh_test_syscall1(long number, long arg1);
long mosh_test_syscall2(long number, long arg1, long arg2);
long mosh_test_syscall3(long number, long arg1, long arg2, long arg3);
void mosh_test_write_bytes(int fd, const char* data, size_t length);
void mosh_test_set_env(char** envp);

static char* g_envp[] = { "PATH=/bin", NULL };

#include "../app/mosh/mosh.c"

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

long mosh_test_syscall0(long number) {
  if(number == SYS_FORK) {
    long pid = 1000 + g_fork_calls;
    g_fork_calls++;
    return pid;
  }
  if(number == SYS_STDIN_POLL) {
    return -1;
  }
  return 0;
}

long mosh_test_syscall1(long number, long arg1) {
  if(number == SYS_PIPE) {
    int* fds = (int*)arg1;
    if(fds != NULL) {
      static int next_fd = 200;
      fds[0] = next_fd++;
      fds[1] = next_fd++;
    }
    return 0;
  }
  return 0;
}

long mosh_test_syscall2(long number, long arg1, long arg2) {
  (void)number;
  (void)arg1;
  (void)arg2;
  return 0;
}

long mosh_test_syscall3(long number, long arg1, long arg2, long arg3) {
  (void)arg3;

  if(number == SYS_WAITPID) {
    int* status_ptr = (int*)arg2;
    if(status_ptr != NULL) {
      *status_ptr = 0;
    }
    g_waitpid_calls++;
    if(g_waitpid_calls == 2) {
      const char output[] = "hi\n";
      mosh_test_write_bytes(STDOUT_FILENO, output, sizeof(output) - 1);
    }
    return arg1;
  }

  if(number == SYS_EXECVE) {
    return 0;
  }

  if(number == SYS_OPEN || number == SYS_DUP2 || number == SYS_PROC_KILL) {
    return 0;
  }

  return -1;
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

static void reset_state(void) {
  g_capture_length = 0;
  g_fork_calls = 0;
  g_waitpid_calls = 0;
  memset(g_capture, 0, sizeof(g_capture));
}

static void run_line(const char* line) {
  char buffer[128];
  memset(buffer, 0, sizeof(buffer));
  strncpy(buffer, line, sizeof(buffer) - 1);
  launch_command(buffer);
}

void setUp(void) {
  reset_state();
  mosh_test_set_env(g_envp);
}

void tearDown(void) {}

void test_pipeline_runs(void) {
  run_line("echo hi | cat");
  const char expected[] = "hi\n";
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0,
                                           find_subsequence(g_capture, g_capture_length, expected, sizeof(expected) - 1),
                                           "Pipeline should forward stdout to next command");
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_pipeline_runs);

  return UNITY_END();
}
