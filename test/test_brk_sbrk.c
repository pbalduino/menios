#include "unity.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static void* initial_break = NULL;

void setUp(void) {
  errno = 0;
  void* current = sbrk(0);
  TEST_ASSERT_NOT_EQUAL_MESSAGE((void*)-1, current, strerror(errno));

  if(initial_break == NULL) {
    initial_break = current;
  } else {
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, brk(initial_break), strerror(errno));
    void* reset = sbrk(0);
    TEST_ASSERT_NOT_EQUAL_MESSAGE((void*)-1, reset, strerror(errno));
    TEST_ASSERT_EQUAL_PTR(initial_break, reset);
  }
  errno = 0;
}

void tearDown(void) {
  errno = 0;
}

void test_sbrk_zero_returns_current_break(void) {
  void* current = sbrk(0);
  TEST_ASSERT_NOT_EQUAL((void*)-1, current);
  TEST_ASSERT_EQUAL_PTR(initial_break, current);
}

void test_sbrk_positive_increment_updates_break(void) {
  char* base = (char*)sbrk(0);
  TEST_ASSERT_NOT_EQUAL((void*)-1, base);

  errno = 0;
  void* prev = sbrk(4096);
  TEST_ASSERT_NOT_EQUAL((void*)-1, prev);
  TEST_ASSERT_EQUAL_PTR(base, prev);
  TEST_ASSERT_EQUAL_PTR(base + 4096, sbrk(0));

  TEST_ASSERT_EQUAL_INT(0, brk(base));
  TEST_ASSERT_EQUAL_PTR(base, sbrk(0));
}

void test_sbrk_negative_increment_shrinks_within_bounds(void) {
  char* base = (char*)sbrk(0);
  TEST_ASSERT_NOT_EQUAL((void*)-1, base);

  errno = 0;
  void* prev = sbrk(8192);
  if(prev == (void*)-1) {
    TEST_IGNORE_MESSAGE("Unable to allocate grow region for shrink test");
    return;
  }

  TEST_ASSERT_EQUAL_PTR(base, prev);
  TEST_ASSERT_EQUAL_PTR(base + 8192, sbrk(0));

  errno = 0;
  void* shrink_prev = sbrk(-4096);
  TEST_ASSERT_NOT_EQUAL((void*)-1, shrink_prev);
  TEST_ASSERT_EQUAL_PTR(base + 8192, shrink_prev);
  TEST_ASSERT_EQUAL_PTR(base + 4096, sbrk(0));

  TEST_ASSERT_EQUAL_INT(0, brk(base));
  TEST_ASSERT_EQUAL_PTR(base, sbrk(0));
}

void test_sbrk_rejects_negative_past_base(void) {
  errno = 0;
  void* result = sbrk(-4096);
  TEST_ASSERT_EQUAL_PTR((void*)-1, result);
  TEST_ASSERT_EQUAL_INT(EINVAL, errno);
  TEST_ASSERT_EQUAL_PTR(initial_break, sbrk(0));
}

void test_sbrk_rejects_excessive_growth(void) {
  errno = 0;
  void* result = sbrk(INTPTR_MAX / 2);
  TEST_ASSERT_EQUAL_PTR((void*)-1, result);
  TEST_ASSERT_EQUAL_INT(ENOMEM, errno);
  TEST_ASSERT_EQUAL_PTR(initial_break, sbrk(0));
}

void test_brk_rejects_address_before_base(void) {
  errno = 0;
  char* base = (char*)initial_break;
  TEST_ASSERT_EQUAL_INT(-1, brk(base - 1));
  TEST_ASSERT_EQUAL_INT(EINVAL, errno);
  TEST_ASSERT_EQUAL_PTR(initial_break, sbrk(0));
}

void test_large_growth_allocates_when_supported(void) {
  char* base = (char*)sbrk(0);
  TEST_ASSERT_NOT_EQUAL((void*)-1, base);

  intptr_t request = 20 * 1024 * 1024;
  errno = 0;
  void* prev = sbrk(request);
  if(prev == (void*)-1) {
    TEST_IGNORE_MESSAGE("Host rejected large sbrk growth; skipping extension test");
    return;
  }

  TEST_ASSERT_EQUAL_PTR(base, prev);
  TEST_ASSERT_EQUAL_PTR(base + request, sbrk(0));

  TEST_ASSERT_EQUAL_INT(0, brk(base));
  TEST_ASSERT_EQUAL_PTR(base, sbrk(0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sbrk_zero_returns_current_break);
  RUN_TEST(test_sbrk_positive_increment_updates_break);
  RUN_TEST(test_sbrk_negative_increment_shrinks_within_bounds);
  RUN_TEST(test_sbrk_rejects_negative_past_base);
  RUN_TEST(test_sbrk_rejects_excessive_growth);
  RUN_TEST(test_brk_rejects_address_before_base);
  RUN_TEST(test_large_growth_allocates_when_supported);
  return UNITY_END();
}
