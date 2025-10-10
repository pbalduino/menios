#include "unity.h"

#include <kernel/syscall.h>
#include <kernel/pmm.h>
#include <menios/syscall.h>

void setUp(void) {
  syscall_init();
}

void tearDown(void) {}

void test_getpagesize_returns_kernel_page_size(void) {
  syscall_frame_t frame = {0};
  frame.rax = SYS_GETPAGESIZE;

  uint64_t rc = syscall_dispatch(&frame);

  TEST_ASSERT_EQUAL_UINT64(PAGE_SIZE, rc);
  TEST_ASSERT_EQUAL_UINT64(PAGE_SIZE, frame.rax);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_getpagesize_returns_kernel_page_size);
  return UNITY_END();
}
