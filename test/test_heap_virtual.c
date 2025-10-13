#include "unity.h"

#include <stddef.h>
#include <types.h>
#include <kernel/heap.h>

void setUp(void) {
  __kmalloc_debug_reset_virtual();
}

void tearDown(void) {}

void test_virtual_range_reuse(void) {
  virt_addr_t initial = __kmalloc_debug_next_vaddr();
  virt_addr_t base1 = 0;
  TEST_ASSERT_TRUE(__kmalloc_debug_reserve_range(0x2000, &base1));
  TEST_ASSERT_EQUAL_UINT64(initial, base1);

  virt_addr_t base2 = 0;
  TEST_ASSERT_TRUE(__kmalloc_debug_reserve_range(0x2000, &base2));
  TEST_ASSERT_EQUAL_UINT64(initial + 0x2000, base2);

  __kmalloc_debug_release_range(base1, 0x2000);

  TEST_ASSERT_EQUAL_size_t(1u, __kmalloc_debug_free_range_count());

  virt_addr_t reuse = 0;
  TEST_ASSERT_TRUE(__kmalloc_debug_reserve_range(0x1000, &reuse));
  TEST_ASSERT_EQUAL_UINT64(base1, reuse);
  TEST_ASSERT_EQUAL_size_t(1u, __kmalloc_debug_free_range_count());

  virt_addr_t reuse_tail = 0;
  TEST_ASSERT_TRUE(__kmalloc_debug_reserve_range(0x1000, &reuse_tail));
  TEST_ASSERT_EQUAL_UINT64(base1 + 0x1000, reuse_tail);
  TEST_ASSERT_EQUAL_size_t(0u, __kmalloc_debug_free_range_count());

  __kmalloc_debug_release_range(reuse, 0x1000);
  __kmalloc_debug_release_range(reuse_tail, 0x1000);
  TEST_ASSERT_EQUAL_size_t(1u, __kmalloc_debug_free_range_count());

  virt_addr_t base3 = 0;
  TEST_ASSERT_TRUE(__kmalloc_debug_reserve_range(0x2000, &base3));
  TEST_ASSERT_EQUAL_UINT64(base1, base3);
  TEST_ASSERT_EQUAL_size_t(0u, __kmalloc_debug_free_range_count());

  __kmalloc_debug_release_range(base2, 0x2000);
  __kmalloc_debug_release_range(base3, 0x2000);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_virtual_range_reuse);
  return UNITY_END();
}
