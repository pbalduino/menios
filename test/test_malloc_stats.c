#include "unity.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef MENIOS_HOST_TEST
#define LARGE_ALLOCATION (16 * 1024 * 1024u)
#else
#define LARGE_ALLOCATION (150 * 1024 * 1024u)
#endif

void setUp(void) {}
void tearDown(void) {}

void test_stats_rejects_null_pointer(void) {
  errno = 0;
  TEST_ASSERT_EQUAL_INT(-1, menios_malloc_stats(NULL));
  TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

void test_stats_track_direct_allocations(void) {
  menios_malloc_stats_t before = {0};
  TEST_ASSERT_EQUAL_INT(0, menios_malloc_stats(&before));

  uint8_t* big = malloc(LARGE_ALLOCATION);
  if(big == NULL) {
    TEST_IGNORE_MESSAGE("malloc returned NULL; skipping direct allocation stats test");
    return;
  }

  menios_malloc_stats_t after = {0};
  TEST_ASSERT_EQUAL_INT(0, menios_malloc_stats(&after));
  TEST_ASSERT_EQUAL_size_t(before.direct_allocations + 1u, after.direct_allocations);
  TEST_ASSERT_TRUE(after.direct_bytes >= before.direct_bytes + LARGE_ALLOCATION);

  free(big);

  menios_malloc_stats_t final = {0};
  TEST_ASSERT_EQUAL_INT(0, menios_malloc_stats(&final));
  TEST_ASSERT_EQUAL_size_t(before.direct_allocations, final.direct_allocations);
  TEST_ASSERT_EQUAL_size_t(before.direct_bytes, final.direct_bytes);
}

void test_stats_reflect_buddy_free_bytes_after_free(void) {
  uint8_t* small = malloc(2048);
  if(small == NULL) {
    TEST_IGNORE_MESSAGE("malloc returned NULL; skipping buddy stats test");
    return;
  }

  menios_malloc_stats_t mid = {0};
  TEST_ASSERT_EQUAL_INT(0, menios_malloc_stats(&mid));
  TEST_ASSERT_TRUE(mid.arena_count >= 1u);

  free(small);

  menios_malloc_stats_t after = {0};
  TEST_ASSERT_EQUAL_INT(0, menios_malloc_stats(&after));
  TEST_ASSERT_TRUE(after.buddy_free_payload_bytes >= mid.buddy_free_payload_bytes);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_stats_rejects_null_pointer);
  RUN_TEST(test_stats_track_direct_allocations);
  RUN_TEST(test_stats_reflect_buddy_free_bytes_after_free);
  return UNITY_END();
}
