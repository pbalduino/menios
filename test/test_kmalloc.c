#include <kernel/pmm.h>
#include <kernel/heap.h>
#include <stdio.h>
#include <unity.h>
#include <errno.h>
#include <stdlib.h>

void* arena1;
void* arena2;

void setUp() {
  arena1 = aligned_alloc(PAGE_SIZE, PAGE_SIZE);
  if(arena1 == NULL) {
    TEST_FAIL_MESSAGE("malloc failed");
  }
  init_heap(arena1, PAGE_SIZE);
}

void tearDown() {
  free(arena1);
}

void test_malloc_WHEN_size_is_zero_SHOULD_return_null() {
  TEST_ASSERT_NULL_MESSAGE(kmalloc(0), "kmalloc doesn't returned NULL");
}

void test_malloc_SHOULD_start_in_a_valid_heap() {
  heap_node_p node;
  uint32_t size = PAGE_SIZE - HEAP_HEADER_SIZE;
  int ok = inspect_heap(0, &node);

  TEST_ASSERT_EQUAL_UINT32_MESSAGE(HEAP_INSPECT_OK, ok, "inspect_heap found an error");
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(HEAP_FREE, node->status, "The heap node should be marked as free");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(size, node->size, "The heap node size doesn't match");
  TEST_ASSERT_NULL_MESSAGE(node->prev, "The heap node should not have a prev node");
  TEST_ASSERT_NULL_MESSAGE(node->next, "The heap node should not have a next node");
  TEST_ASSERT_NOT_NULL_MESSAGE(node->data, "The heap node data should not be null");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(HEAP_MAGIC, node->magic, "The magic value doesn't match");
}

void test_malloc_WHEN_size_is_positive_SHOULD_return_a_valid_node() {
  uint32_t size = 16;
  heap_node_p node;

  TEST_ASSERT_NOT_NULL_MESSAGE(kmalloc(size), "kmalloc returned NULL");

  int ok = inspect_heap(0, &node);

  TEST_ASSERT_EQUAL_UINT32_MESSAGE(HEAP_INSPECT_OK, ok, "inspect_heap found an error");
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(HEAP_USED, node->status, "The heap node should be marked as used");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(size, node->size, "The heap node size doesn't match");
  TEST_ASSERT_NULL_MESSAGE(node->prev, "The first heap node should not have a prev node");
  TEST_ASSERT_NOT_NULL_MESSAGE(node->next, "The heap node should have a next node");
  TEST_ASSERT_NOT_NULL_MESSAGE(node->data, "The heap node data should not be null");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(HEAP_MAGIC, node->magic, "The magic value doesn't match");

  printf("Arena1: %p\n", arena1);

  for(int i = 1; i < 23; i++) {
    printf("%2d: %p\n", i, kmalloc(16 * i));
  }

  debug_heap((heap_node_p)arena1);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_malloc_WHEN_size_is_zero_SHOULD_return_null);
  RUN_TEST(test_malloc_SHOULD_start_in_a_valid_heap);
  RUN_TEST(test_malloc_WHEN_size_is_positive_SHOULD_return_a_valid_node);

  return UNITY_END();
}