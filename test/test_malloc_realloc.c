#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void fill_pattern(uint8_t* buf, size_t len, uint8_t start) {
  for(size_t i = 0; i < len; ++i) {
    buf[i] = (uint8_t)(start + (uint8_t)i);
  }
}

static void expect_pattern(const uint8_t* buf, size_t len, uint8_t start) {
  for(size_t i = 0; i < len; ++i) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(start + (uint8_t)i), buf[i]);
  }
}

void test_realloc_shrinks_and_releases_tail(void) {
  const size_t original_size = 32768;
  uint8_t* block = malloc(original_size);
  TEST_ASSERT_NOT_NULL(block);

  fill_pattern(block, original_size, 0x10);

  uint8_t* shrunk = realloc(block, 512);
  TEST_ASSERT_NOT_NULL(shrunk);
  expect_pattern(shrunk, 512, 0x10);

  uint8_t* tail = malloc(original_size / 2);
  TEST_ASSERT_NOT_NULL(tail);
  free(tail);

  free(shrunk);
}

void test_realloc_expands_and_preserves_contents(void) {
  uint8_t* block = malloc(2048);
  TEST_ASSERT_NOT_NULL(block);
  fill_pattern(block, 2048, 0x33);

  size_t target = 2048 + 4096;
  uint8_t* grown = realloc(block, target);
  TEST_ASSERT_NOT_NULL(grown);
  expect_pattern(grown, 2048, 0x33);

  memset(grown + 2048, 0x55, target - 2048);
  free(grown);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_realloc_shrinks_and_releases_tail);
  RUN_TEST(test_realloc_expands_and_preserves_contents);
  return UNITY_END();
}
