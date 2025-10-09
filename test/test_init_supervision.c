#include <unity.h>

#include <kernel/proc.h>
#include <kernel/syscall.h>
#include <sys/wait.h>

static cpu_state_t parent_frame;
static cpu_state_t child_frame;
static proc_info_t parent_proc;
static proc_info_t child_proc;
static proc_info_t *old_current;
static size_t old_children;
static proc_info_p old_first_child;
static int status_slot;
static syscall_frame_t frame;
static bool syscalls_ready = false;

void setUp(void) {
  if(!syscalls_ready) {
    syscall_init();
    syscalls_ready = true;
  }

  old_current = current;
  current = &parent_proc;
  parent_proc.parent = NULL;
  parent_proc.children_count = 0;
  parent_proc.first_child = NULL;
  parent_proc.waitpid_target = -1;
  parent_proc.waitpid_waiting = false;
  parent_proc.state = PROC_STATE_RUNNING;
  parent_proc.cpu_state = &parent_frame;
  parent_proc.entrypoint = NULL;
  parent_proc.arguments = NULL;
  parent_proc.pid = 1;
  parent_proc.priority = PROC_PRIO_NORMAL;
  parent_proc.quantum_us = 1000;
  parent_proc.time_slice_remaining_us = parent_proc.quantum_us;
  parent_proc.sleep_until = 0;

  child_proc.parent = &parent_proc;
  child_proc.children_count = 0;
  child_proc.first_child = NULL;
  child_proc.waitpid_target = -1;
  child_proc.waitpid_waiting = false;
  child_proc.state = PROC_STATE_RUNNING;
  child_proc.cpu_state = &child_frame;
  child_proc.entrypoint = NULL;
  child_proc.arguments = NULL;
  child_proc.pid = 2;
  child_proc.priority = PROC_PRIO_NORMAL;
  child_proc.quantum_us = 1000;
  child_proc.time_slice_remaining_us = child_proc.quantum_us;
  child_proc.sleep_until = 0;

  parent_proc.first_child = &child_proc;
  parent_proc.children_count = 1;

  old_children = parent_proc.children_count;
  old_first_child = parent_proc.first_child;

  status_slot = 0;
  frame.rdi = (uint64_t)child_proc.pid;
  frame.rsi = (uint64_t)&status_slot;
  frame.rdx = 0;
}

void tearDown(void) {
  current = old_current;
}

static void run_waitpid(void) {
  frame.rax = SYS_WAITPID;
  syscall_dispatch(&frame);
}

void test_waitpid_nonblocking_pending_child(void) {
  frame.rdx = WNOHANG;
  child_proc.state = PROC_STATE_RUNNING;

  run_waitpid();

  TEST_ASSERT_EQUAL_UINT64(0, frame.rax);
  TEST_ASSERT_TRUE(parent_proc.waitpid_waiting);
  TEST_ASSERT_EQUAL_INT(child_proc.pid, parent_proc.waitpid_target);
  TEST_ASSERT_EQUAL(PROC_STATE_WAITING, parent_proc.state);
  TEST_ASSERT_EQUAL(old_children, parent_proc.children_count);
  TEST_ASSERT_EQUAL_PTR(old_first_child, parent_proc.first_child);
  TEST_ASSERT_EQUAL_INT(0, status_slot);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_waitpid_nonblocking_pending_child);
  return UNITY_END();
}
