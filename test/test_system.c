#include "unity.h"

#include <errno.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
  FORK_MODE_PARENT = 0,
  FORK_MODE_CHILD,
  FORK_MODE_FAIL
} fork_mode_t;

static fork_mode_t fork_mode;
static pid_t       fork_parent_pid;
static int         fork_errno_value;
static int         fork_call_count;

static int   waitpid_call_count;
static int   waitpid_eintr_limit;
static int   waitpid_errno_value;
static int   waitpid_status_value;
static pid_t waitpid_return_value;

static int  access_call_count;
static int  access_result_value;
static int  access_errno_value;
static int  access_last_mode;
static char access_last_path[128];

static int   getenv_call_count;
static char* getenv_result_value;
static char  getenv_last_name[32];

static int   execve_call_count;
static int   execve_result_value;
static int   execve_errno_value;
static char  execve_last_path[128];
static char  execve_last_arg0[128];
static char  execve_last_arg1[16];
static char  execve_last_arg2[256];
static int   execve_last_argc;
static char* const* execve_last_envp;

static int     exit_call_count;
static int     exit_last_status;
static jmp_buf exit_jump_buffer;
static int     exit_jump_active;

extern char** environ;

void setUp(void) {
  fork_mode = FORK_MODE_PARENT;
  fork_parent_pid = 1234;
  fork_errno_value = 0;
  fork_call_count = 0;

  waitpid_call_count = 0;
  waitpid_eintr_limit = 0;
  waitpid_errno_value = 0;
  waitpid_status_value = 0;
  waitpid_return_value = 0;

  access_call_count = 0;
  access_result_value = 0;
  access_errno_value = 0;
  access_last_mode = 0;
  access_last_path[0] = '\0';

  getenv_call_count = 0;
  getenv_result_value = NULL;
  getenv_last_name[0] = '\0';

  execve_call_count = 0;
  execve_result_value = -1;
  execve_errno_value = ENOENT;
  execve_last_path[0] = '\0';
  execve_last_arg0[0] = '\0';
  execve_last_arg1[0] = '\0';
  execve_last_arg2[0] = '\0';
  execve_last_argc = 0;
  execve_last_envp = NULL;

  exit_call_count = 0;
  exit_last_status = -1;
  exit_jump_active = 0;
}

void tearDown(void) {}

pid_t __menios_system_fork(void) {
  fork_call_count++;
  if(fork_mode == FORK_MODE_FAIL) {
    errno = fork_errno_value;
    return -1;
  }
  if(fork_mode == FORK_MODE_CHILD) {
    return 0;
  }
  return fork_parent_pid;
}

int __menios_system_execve(const char* path, char* const argv[], char* const envp[]) {
  execve_call_count++;
  if(path != NULL) {
    strncpy(execve_last_path, path, sizeof(execve_last_path) - 1);
    execve_last_path[sizeof(execve_last_path) - 1] = '\0';
  } else {
    execve_last_path[0] = '\0';
  }

  execve_last_argc = 0;
  if(argv != NULL) {
    if(argv[0] != NULL) {
      strncpy(execve_last_arg0, argv[0], sizeof(execve_last_arg0) - 1);
      execve_last_arg0[sizeof(execve_last_arg0) - 1] = '\0';
      execve_last_argc++;
    } else {
      execve_last_arg0[0] = '\0';
    }

    if(argv[1] != NULL) {
      strncpy(execve_last_arg1, argv[1], sizeof(execve_last_arg1) - 1);
      execve_last_arg1[sizeof(execve_last_arg1) - 1] = '\0';
      execve_last_argc++;
    } else {
      execve_last_arg1[0] = '\0';
    }

    if(argv[2] != NULL) {
      strncpy(execve_last_arg2, argv[2], sizeof(execve_last_arg2) - 1);
      execve_last_arg2[sizeof(execve_last_arg2) - 1] = '\0';
      execve_last_argc++;
    } else {
      execve_last_arg2[0] = '\0';
    }
  }

  execve_last_envp = envp;
  errno = execve_errno_value;
  return execve_result_value;
}

pid_t __menios_system_waitpid(pid_t pid, int* status, int options) {
  (void)options;
  waitpid_call_count++;

  if(waitpid_call_count <= waitpid_eintr_limit) {
    errno = EINTR;
    return -1;
  }

  if(waitpid_errno_value != 0) {
    errno = waitpid_errno_value;
    return -1;
  }

  if(status != NULL) {
    *status = waitpid_status_value;
  }
  errno = 0;
  if(waitpid_return_value != 0) {
    return waitpid_return_value;
  }
  return pid;
}

int __menios_system_access(const char* path, int mode) {
  access_call_count++;
  access_last_mode = mode;
  if(path != NULL) {
    strncpy(access_last_path, path, sizeof(access_last_path) - 1);
    access_last_path[sizeof(access_last_path) - 1] = '\0';
  } else {
    access_last_path[0] = '\0';
  }

  if(access_result_value != 0) {
    errno = access_errno_value;
  }
  return access_result_value;
}

char* __menios_system_getenv(const char* name) {
  getenv_call_count++;
  if(name != NULL) {
    strncpy(getenv_last_name, name, sizeof(getenv_last_name) - 1);
    getenv_last_name[sizeof(getenv_last_name) - 1] = '\0';
  } else {
    getenv_last_name[0] = '\0';
  }
  return getenv_result_value;
}

void __menios_system_exit(int status) {
  exit_call_count++;
  exit_last_status = status;
  if(exit_jump_active) {
    longjmp(exit_jump_buffer, 1);
  }
}

void test_system_null_returns_one_when_shell_accessible(void) {
  getenv_result_value = NULL;
  access_result_value = 0;
  errno = 42;

  int rc = system(NULL);

  TEST_ASSERT_EQUAL_INT(1, rc);
  TEST_ASSERT_EQUAL_INT(42, errno);
  TEST_ASSERT_EQUAL_INT(1, getenv_call_count);
  TEST_ASSERT_EQUAL_STRING("SHELL", getenv_last_name);
  TEST_ASSERT_EQUAL_INT(1, access_call_count);
  TEST_ASSERT_EQUAL_STRING("/bin/mosh", access_last_path);
  TEST_ASSERT_EQUAL_INT(X_OK, access_last_mode);
  TEST_ASSERT_EQUAL_INT(0, fork_call_count);
}

void test_system_null_returns_zero_when_shell_missing(void) {
  getenv_result_value = NULL;
  access_result_value = -1;
  access_errno_value = ENOENT;
  errno = 99;

  int rc = system(NULL);

  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_INT(99, errno);
  TEST_ASSERT_EQUAL_INT(1, access_call_count);
  TEST_ASSERT_EQUAL_STRING("/bin/mosh", access_last_path);
}

void test_system_null_prefers_shell_environment(void) {
  static char shell_value[] = "/custom/shell";
  getenv_result_value = shell_value;
  access_result_value = 0;

  int rc = system(NULL);

  TEST_ASSERT_EQUAL_INT(1, rc);
  TEST_ASSERT_EQUAL_INT(1, getenv_call_count);
  TEST_ASSERT_EQUAL_STRING(shell_value, access_last_path);
}

void test_system_returns_child_status_after_wait(void) {
  static char shell_value[] = "/custom/shell";
  getenv_result_value = shell_value;
  fork_mode = FORK_MODE_PARENT;
  fork_parent_pid = 4242;
  waitpid_eintr_limit = 1;
  waitpid_status_value = (7 << 8);
  waitpid_return_value = 4242;

  int rc = system("echo hi");

  TEST_ASSERT_EQUAL_INT(waitpid_status_value, rc);
  TEST_ASSERT_EQUAL_INT(1, fork_call_count);
  TEST_ASSERT_EQUAL_INT(waitpid_eintr_limit + 1, waitpid_call_count);
  TEST_ASSERT_EQUAL_INT(0, execve_call_count);
  TEST_ASSERT_EQUAL_INT(1, getenv_call_count);
}

void test_system_propagates_waitpid_failure(void) {
  fork_mode = FORK_MODE_PARENT;
  fork_parent_pid = 111;
  waitpid_errno_value = ECHILD;

  int rc = system("echo hi");

  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_EQUAL_INT(ECHILD, errno);
  TEST_ASSERT_EQUAL_INT(1, fork_call_count);
  TEST_ASSERT_EQUAL_INT(1, waitpid_call_count);
}

void test_system_propagates_fork_failure(void) {
  fork_mode = FORK_MODE_FAIL;
  fork_errno_value = EAGAIN;

  int rc = system("echo hi");

  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_EQUAL_INT(EAGAIN, errno);
  TEST_ASSERT_EQUAL_INT(1, fork_call_count);
  TEST_ASSERT_EQUAL_INT(0, waitpid_call_count);
}

void test_system_child_executes_shell_command(void) {
  static char shell_value[] = "/custom/sh";
  static char command_value[] = "run-something";
  getenv_result_value = shell_value;
  fork_mode = FORK_MODE_CHILD;
  execve_result_value = -1;
  execve_errno_value = ENOENT;

  int jump_rc = setjmp(exit_jump_buffer);
  if(jump_rc == 0) {
    exit_jump_active = 1;
    (void)system(command_value);
    TEST_FAIL_MESSAGE("system() returned unexpectedly in child branch");
  }

  exit_jump_active = 0;

  TEST_ASSERT_EQUAL_INT(1, execve_call_count);
  TEST_ASSERT_EQUAL_STRING(shell_value, execve_last_path);
  TEST_ASSERT_EQUAL_INT(3, execve_last_argc);
  TEST_ASSERT_EQUAL_STRING(shell_value, execve_last_arg0);
  TEST_ASSERT_EQUAL_STRING("-c", execve_last_arg1);
  TEST_ASSERT_EQUAL_STRING(command_value, execve_last_arg2);
  TEST_ASSERT_NOT_NULL(execve_last_envp);
  TEST_ASSERT_EQUAL_PTR(environ, execve_last_envp);
  TEST_ASSERT_EQUAL_INT(1, exit_call_count);
  TEST_ASSERT_EQUAL_INT(127, exit_last_status);
}

void test_system_falls_back_when_shell_env_empty(void) {
  static char empty_shell[] = "";
  getenv_result_value = empty_shell;
  access_result_value = 0;

  int rc = system(NULL);

  TEST_ASSERT_EQUAL_INT(1, rc);
  TEST_ASSERT_EQUAL_STRING("/bin/mosh", access_last_path);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_system_null_returns_one_when_shell_accessible);
  RUN_TEST(test_system_null_returns_zero_when_shell_missing);
  RUN_TEST(test_system_null_prefers_shell_environment);
  RUN_TEST(test_system_returns_child_status_after_wait);
  RUN_TEST(test_system_propagates_waitpid_failure);
  RUN_TEST(test_system_propagates_fork_failure);
  RUN_TEST(test_system_child_executes_shell_command);
  RUN_TEST(test_system_falls_back_when_shell_env_empty);
  return UNITY_END();
}
