#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>
#include <menios/syscall.h>

static int kill_syscall_calls;
static long kill_last_pid;
static long kill_last_code;

long __menios_syscall2(long number, long arg1, long arg2) {
  TEST_ASSERT_EQUAL_MESSAGE(SYS_PROC_KILL, number, "kill should call SYS_PROC_KILL");
  kill_syscall_calls++;
  kill_last_pid = arg1;
  kill_last_code = arg2;
  return 0;
}

#define main kill_main
#include "../app/kill/kill.c"
#undef main

void setUp(void) {
  kill_syscall_calls = 0;
  kill_last_pid = 0;
  kill_last_code = 0;
}

void tearDown(void) {}

void test_kill_invokes_syscall_with_defaults(void) {
  char* argv[] = { "kill", "42", NULL };
  int rc = kill_main(2, argv);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_INT(1, kill_syscall_calls);
  TEST_ASSERT_EQUAL_INT(42, kill_last_pid);
  TEST_ASSERT_EQUAL_INT(0, kill_last_code);
}

void test_kill_accepts_custom_code(void) {
  char* argv[] = { "kill", "7", "99", NULL };
  int rc = kill_main(3, argv);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_INT(1, kill_syscall_calls);
  TEST_ASSERT_EQUAL_INT(7, kill_last_pid);
  TEST_ASSERT_EQUAL_INT(99, kill_last_code);
}

void test_kill_invalid_pid_fails(void) {
  char* argv[] = { "kill", "abc", NULL };
  int rc = kill_main(2, argv);
  TEST_ASSERT_NOT_EQUAL(0, rc);
  TEST_ASSERT_EQUAL_INT(0, kill_syscall_calls);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_kill_invokes_syscall_with_defaults);
  RUN_TEST(test_kill_accepts_custom_code);
  RUN_TEST(test_kill_invalid_pid_fails);
  return UNITY_END();
}
