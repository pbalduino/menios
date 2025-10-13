#include "unity.h"

#include <stdint.h>
#include <stdlib.h>

#include "../user/libc/allocator_debug.h"

#define BUDDY_MIN_ORDER 7u
#define BUDDY_MAX_ORDER 27u

static void initialise_arena(void) {
  if(__menios_allocator_grow_heap_for_test(1u << 12) != 0) {
    TEST_IGNORE_MESSAGE("grow_heap failed; skipping buddy allocator tests");
    return;
  }
}

void setUp(void) {
  __menios_allocator_reset();
  initialise_arena();
}

void tearDown(void) {
  __menios_allocator_reset();
}

void test_buddy_split_down_to_target(void) {
  const uint32_t target_order = 20u;
  TEST_ASSERT_EQUAL_size_t(1u, __menios_buddy_debug_freelist_length(BUDDY_MAX_ORDER));
  block_header_t* root = __menios_buddy_debug_pop(BUDDY_MAX_ORDER);
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_UINT32(BUDDY_MAX_ORDER, __menios_buddy_debug_order(root));
  uintptr_t root_offset = __menios_buddy_debug_offset(root);

  block_header_t* block = __menios_buddy_debug_split(root, target_order);
  TEST_ASSERT_NOT_NULL(block);
  TEST_ASSERT_EQUAL_UINT32(target_order, __menios_buddy_debug_order(block));
  TEST_ASSERT_EQUAL_UINT64(root_offset, __menios_buddy_debug_offset(block));

  for(uint32_t order = target_order; order < BUDDY_MAX_ORDER; ++order) {
    TEST_ASSERT_EQUAL_size_t(1u, __menios_buddy_debug_freelist_length(order));
  }

  for(uint32_t order = BUDDY_MIN_ORDER; order < target_order; ++order) {
    TEST_ASSERT_EQUAL_size_t(0u, __menios_buddy_debug_freelist_length(order));
  }

  __menios_buddy_debug_push(block);
}

void test_buddy_coalesce_merges_to_root(void) {
  const uint32_t target_order = 18u;
  TEST_ASSERT_EQUAL_size_t(1u, __menios_buddy_debug_freelist_length(BUDDY_MAX_ORDER));
  block_header_t* root = __menios_buddy_debug_pop(BUDDY_MAX_ORDER);
  TEST_ASSERT_NOT_NULL(root);

  block_header_t* block = __menios_buddy_debug_split(root, target_order);
  TEST_ASSERT_NOT_NULL(block);
  TEST_ASSERT_EQUAL_UINT32(target_order, __menios_buddy_debug_order(block));

  block_header_t* merged = __menios_buddy_debug_coalesce(block);
  TEST_ASSERT_NOT_NULL(merged);
  TEST_ASSERT_EQUAL_UINT32(BUDDY_MAX_ORDER, __menios_buddy_debug_order(merged));
  TEST_ASSERT_EQUAL_size_t(1u, __menios_buddy_debug_freelist_length(BUDDY_MAX_ORDER));

  for(uint32_t order = BUDDY_MIN_ORDER; order < BUDDY_MAX_ORDER; ++order) {
    TEST_ASSERT_EQUAL_size_t(0u, __menios_buddy_debug_freelist_length(order));
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_buddy_split_down_to_target);
  RUN_TEST(test_buddy_coalesce_merges_to_root);
  return UNITY_END();
}
