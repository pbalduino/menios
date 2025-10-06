#include <kernel/proc.h>
#include <unity.h>

#include <string.h>

extern proc_info_p current;

static proc_info_t parent_proc;
static proc_info_t child_proc;

void setUp(void) {
  memset(&parent_proc, 0, sizeof(parent_proc));
  memset(&child_proc, 0, sizeof(child_proc));

  parent_proc.pid = 1;
  parent_proc.state = PROC_STATE_RUNNING;
  parent_proc.first_child = &child_proc;
  parent_proc.children_count = 1;

  child_proc.pid = 2;
  child_proc.parent = &parent_proc;
  child_proc.sibling_next = NULL;
}

void tearDown(void) {
}

void test_waitpid_returns_child_when_zombie(void) {
  child_proc.state = PROC_STATE_ZOMBIE;
  child_proc.exit_code = 42;

  int status = 0;
  int result = proc_waitpid(&parent_proc, child_proc.pid, &status);

  TEST_ASSERT_EQUAL_INT(child_proc.pid, result);
  TEST_ASSERT_EQUAL_INT(42, status);
  TEST_ASSERT_EQUAL_UINT(0, parent_proc.children_count);
  TEST_ASSERT_NULL(parent_proc.first_child);
  TEST_ASSERT_EQUAL(PROC_STATE_TERMINATED, child_proc.state);
}

void test_waitpid_returns_zero_when_child_running(void) {
  child_proc.state = PROC_STATE_RUNNING;
  int status = -1;

  int result = proc_waitpid(&parent_proc, child_proc.pid, &status);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(-1, status);
  TEST_ASSERT_EQUAL_UINT(1, parent_proc.children_count);
  TEST_ASSERT_EQUAL_PTR(&child_proc, parent_proc.first_child);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_waitpid_returns_child_when_zombie);
  RUN_TEST(test_waitpid_returns_zero_when_child_running);

  return UNITY_END();
}
