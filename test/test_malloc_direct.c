#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LARGE_ALLOCATION (150 * 1024 * 1024u)
#define LARGE_ALIGNMENT  (1u << 20) /* 1 MiB */

void setUp(void) {}
void tearDown(void) {}

void test_large_malloc_falls_back_to_direct_mapping(void) {
  uint8_t* block = malloc(LARGE_ALLOCATION);
  TEST_ASSERT_NOT_NULL(block);

  block[0] = 0xAB;
  block[LARGE_ALLOCATION - 1] = 0xCD;
  TEST_ASSERT_EQUAL_UINT8(0xAB, block[0]);
  TEST_ASSERT_EQUAL_UINT8(0xCD, block[LARGE_ALLOCATION - 1]);

  free(block);
}

void test_posix_memalign_high_alignment_uses_direct_mapping(void) {
  void* ptr = NULL;
  int rc = posix_memalign(&ptr, LARGE_ALIGNMENT, 4096);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_NOT_NULL(ptr);
  TEST_ASSERT_EQUAL_UINT64(0u, (uintptr_t)ptr % LARGE_ALIGNMENT);

  memset(ptr, 0x5A, 4096);
  free(ptr);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_large_malloc_falls_back_to_direct_mapping);
  RUN_TEST(test_posix_memalign_high_alignment_uses_direct_mapping);
  return UNITY_END();
}
