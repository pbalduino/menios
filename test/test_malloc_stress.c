#include "unity.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SLOT_COUNT 256
#define STRESS_ITERATIONS 5000
#define MAX_ALLOC_SIZE 8192

static uint32_t lcg_next(uint32_t* state) {
  *state = (*state * 1664525u) + 1013904223u;
  return *state;
}

static void fill_pattern(void* ptr, size_t size, uint8_t pattern) {
  memset(ptr, (int)pattern, size);
}

static void expect_pattern(const uint8_t* ptr, size_t size, uint8_t pattern) {
  for(size_t i = 0; i < size; ++i) {
    TEST_ASSERT_EQUAL_UINT8(pattern, ptr[i]);
  }
}

void setUp(void) {}
void tearDown(void) {}

void test_allocator_handles_fragmented_workload(void) {
  uint8_t* slots[SLOT_COUNT] = { 0 };
  size_t sizes[SLOT_COUNT] = { 0 };
  uint8_t patterns[SLOT_COUNT] = { 0 };
  uint32_t state = 0xC0FFEEu;

  for(size_t iter = 0; iter < STRESS_ITERATIONS; ++iter) {
    uint32_t r = lcg_next(&state);
    size_t index = r % SLOT_COUNT;

    if(slots[index] != NULL) {
      expect_pattern(slots[index], sizes[index], patterns[index]);

      if((r & 0x3u) == 0u) {
        size_t new_size = (lcg_next(&state) % MAX_ALLOC_SIZE) + 1u;
        uint8_t new_pattern = (uint8_t)(new_size & 0xFFu);
        uint8_t* grown = realloc(slots[index], new_size);
        TEST_ASSERT_NOT_NULL(grown);
        expect_pattern(grown, sizes[index] < new_size ? sizes[index] : new_size, patterns[index]);
        fill_pattern(grown, new_size, new_pattern);
        slots[index] = grown;
        sizes[index] = new_size;
        patterns[index] = new_pattern;
        continue;
      }

      free(slots[index]);
      slots[index] = NULL;
    }

    size_t size = (lcg_next(&state) % MAX_ALLOC_SIZE) + 1u;
    uint8_t pattern = (uint8_t)(size & 0xFFu);
    uint8_t* block = malloc(size);
    TEST_ASSERT_NOT_NULL(block);
    fill_pattern(block, size, pattern);
    slots[index] = block;
    sizes[index] = size;
    patterns[index] = pattern;

    size_t usable = malloc_usable_size(block);
    TEST_ASSERT_TRUE(usable >= size);
  }

  for(size_t i = 0; i < SLOT_COUNT; ++i) {
    if(slots[i] != NULL) {
      expect_pattern(slots[i], sizes[i], patterns[i]);
      free(slots[i]);
    }
  }
}

void test_posix_memalign_various_alignments(void) {
  const size_t alignments[] = { 16u, 32u, 64u, 128u, 256u, 4096u };
  uint32_t state = 0xBADC0DEu;

  for(size_t i = 0; i < sizeof(alignments) / sizeof(alignments[0]); ++i) {
    for(size_t attempt = 0; attempt < 32u; ++attempt) {
      size_t alignment = alignments[i];
      size_t size = ((lcg_next(&state) % MAX_ALLOC_SIZE) + alignment);
      void* ptr = NULL;
      int rc = posix_memalign(&ptr, alignment, size);
      TEST_ASSERT_EQUAL_INT(0, rc);
      TEST_ASSERT_NOT_NULL(ptr);
      TEST_ASSERT_EQUAL_UINT64(0u, (uintptr_t)ptr % alignment);
      size_t usable = malloc_usable_size(ptr);
      TEST_ASSERT_TRUE(usable >= size);
      fill_pattern(ptr, size, (uint8_t)(alignment & 0xFFu));
      free(ptr);
    }
  }
}

void test_reallocarray_detects_overflow_conditions(void) {
  void* ptr = malloc(16u);
  TEST_ASSERT_NOT_NULL(ptr);
  errno = 0;
  void* grown = reallocarray(ptr, SIZE_MAX, 4u);
  TEST_ASSERT_NULL(grown);
  TEST_ASSERT_EQUAL_INT(ENOMEM, errno);
  free(ptr);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_allocator_handles_fragmented_workload);
  RUN_TEST(test_posix_memalign_various_alignments);
  RUN_TEST(test_reallocarray_detects_overflow_conditions);
  return UNITY_END();
}
