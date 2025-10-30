#include <kernel/pmm.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <stdio.h>
#include <unity.h>
#include <stdlib.h>
#include <string.h>

void* arena1;

void setUp() {
  current = (proc_info_p)malloc(sizeof(proc_info_t));
  
  arena1 = aligned_alloc(PAGE_SIZE, PAGE_SIZE);
  if(arena1 == NULL) {
    TEST_FAIL_MESSAGE("malloc failed");
  }
  heap_init(arena1, PAGE_SIZE);
}

void tearDown() {
  free(arena1);
}

void test_kmalloc_WHEN_size_is_zero_SHOULD_return_null() {
  TEST_ASSERT_NULL_MESSAGE(kmalloc(0), "kmalloc doesn't returned NULL");
}

void test_kmalloc_SHOULD_start_in_a_valid_heap() {
  heap_node_p node;
  uint32_t size = PAGE_SIZE - HEAP_HEADER_SIZE;
  int ok = inspect_heap(0, &node);

  TEST_ASSERT_EQUAL_UINT32_MESSAGE(HEAP_INSPECT_OK, ok, "inspect_heap found an error");
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(HEAP_FREE, node->status, "The heap node should be marked as free");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(size, node->size, "The heap node size doesn't match");
  TEST_ASSERT_NULL_MESSAGE(node->next, "The heap node should not have a next node");
  TEST_ASSERT_NOT_NULL_MESSAGE(node->data, "The heap node data should not be null");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(HEAP_MAGIC, node->magic, "The magic value doesn't match");
}

void test_kmalloc_WHEN_size_is_positive_SHOULD_return_a_valid_node() {
  size_t size = 16;
  heap_node_p node;

  void* obj = kmalloc(size);
  TEST_ASSERT_NOT_NULL_MESSAGE(obj, "kmalloc returned NULL");
  memset(obj, 0xaa, size);

  int ok = inspect_heap(0, &node);

  TEST_ASSERT_EQUAL_UINT32_MESSAGE(HEAP_INSPECT_OK, ok, "inspect_heap found an error");
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(HEAP_USED, node->status, "The heap node should be marked as used");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(size, node->size, "The heap node size doesn't match");
  TEST_ASSERT_NOT_NULL_MESSAGE(node->next, "The heap node should have a next node");
  TEST_ASSERT_NOT_NULL_MESSAGE(node->data, "The heap node data should not be null");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(HEAP_MAGIC, node->magic, "The magic value doesn't match");

  size = 24;

  for(int i = 1; i < 23; i++) {
    obj = kmalloc(size);
    memset(obj, 0xaa, size);
  }
}

void test_kmalloc_WHEN_arena_is_full_SHOULD_return_null() {
  const size_t chunk = 64;
  void* ptr;
  size_t allocations = 0;

  while((ptr = kmalloc(chunk)) != NULL) {
    memset(ptr, 0xcd, chunk);
    allocations++;
  }

  TEST_ASSERT_NULL(ptr);
  TEST_ASSERT_GREATER_THAN_UINT32(0, allocations);
  TEST_ASSERT_NULL(kmalloc(chunk));
}

void test_kmalloc_SHOULD_not_corrupt_adjacent_blocks() {
  uint8_t* first = (uint8_t*)kmalloc(48);
  uint8_t* second = (uint8_t*)kmalloc(48);

  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_NOT_NULL(second);

  memset(first, 0x11, 48);
  memset(second, 0x22, 48);

  for(size_t i = 0; i < 48; i++) {
    TEST_ASSERT_EQUAL_UINT8(0x11, first[i]);
    TEST_ASSERT_EQUAL_UINT8(0x22, second[i]);
  }
}

void test_kfree_SHOULD_merge_contiguous_nodes() {
  void* a = kmalloc(80);
  void* b = kmalloc(80);
  void* c = kmalloc(80);

  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_NOT_NULL(b);
  TEST_ASSERT_NOT_NULL(c);

  kfree(b);

  heap_node_p second;
  TEST_ASSERT_EQUAL_UINT32(HEAP_INSPECT_OK, inspect_heap(1, &second));
  TEST_ASSERT_EQUAL_UINT8(HEAP_FREE, second->status);

  kfree(a);

  heap_node_p merged;
  TEST_ASSERT_EQUAL_UINT32(HEAP_INSPECT_OK, inspect_heap(0, &merged));
  TEST_ASSERT_EQUAL_UINT8(HEAP_FREE, merged->status);

  kfree(c);

  heap_node_p whole_heap;
  TEST_ASSERT_EQUAL_UINT32(HEAP_INSPECT_OK, inspect_heap(0, &whole_heap));
  TEST_ASSERT_EQUAL_UINT8(HEAP_FREE, whole_heap->status);
  TEST_ASSERT_EQUAL_UINT32(PAGE_SIZE - HEAP_HEADER_SIZE, whole_heap->size);
}

void test_kcalloc_SHOULD_return_zeroed_buffer() {
  uint8_t* buffer = (uint8_t*)kcalloc(8, sizeof(uint8_t));

  TEST_ASSERT_NOT_NULL_MESSAGE(buffer, "kcalloc returned NULL");

  for(size_t i = 0; i < 8; i++) {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, buffer[i], "kcalloc did not zero memory");
  }
}

void test_krealloc_WHEN_expanding_SHOULD_preserve_original_bytes() {
  uint8_t* buffer = (uint8_t*)kmalloc(32);
  TEST_ASSERT_NOT_NULL(buffer);

  for(size_t i = 0; i < 32; i++) {
    buffer[i] = (uint8_t)(i + 1);
  }

  uint8_t* resized = (uint8_t*)krealloc(buffer, 96);
  TEST_ASSERT_NOT_NULL(resized);

  for(size_t i = 0; i < 32; i++) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(i + 1), resized[i]);
  }
}

void test_heap_get_stats_SHOULD_reflect_allocations() {
  heap_stats_t before = heap_get_stats();

  TEST_ASSERT_TRUE(before.total_bytes >= before.free_bytes);

  void* ptr = kmalloc(64);
  TEST_ASSERT_NOT_NULL(ptr);

  heap_stats_t after = heap_get_stats();

  const size_t expected_delta = 64 + HEAP_HEADER_SIZE;

  TEST_ASSERT_EQUAL_UINT64(before.total_bytes, after.total_bytes);
  TEST_ASSERT_EQUAL_UINT64(before.used_bytes + expected_delta, after.used_bytes);
  TEST_ASSERT_EQUAL_UINT64(before.free_bytes - expected_delta, after.free_bytes);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_kmalloc_WHEN_size_is_zero_SHOULD_return_null);
  RUN_TEST(test_kmalloc_SHOULD_start_in_a_valid_heap);
  RUN_TEST(test_kmalloc_WHEN_size_is_positive_SHOULD_return_a_valid_node);
  RUN_TEST(test_kmalloc_WHEN_arena_is_full_SHOULD_return_null);
  RUN_TEST(test_kmalloc_SHOULD_not_corrupt_adjacent_blocks);
  RUN_TEST(test_kfree_SHOULD_merge_contiguous_nodes);
  RUN_TEST(test_kcalloc_SHOULD_return_zeroed_buffer);
  RUN_TEST(test_krealloc_WHEN_expanding_SHOULD_preserve_original_bytes);
  RUN_TEST(test_heap_get_stats_SHOULD_reflect_allocations);

  return UNITY_END();
}
