#include "unity.h"

#include <kernel/shm.h>
#include <kernel/pmm.h>
#include <kernel/heap.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static phys_frame_t fake_alloc(size_t page_count) {
  size_t bytes = page_count * PAGE_SIZE;
  void* block = NULL;
  if(posix_memalign(&block, PAGE_SIZE, bytes) != 0 || block == NULL) {
    return phys_frame_invalid();
  }
  memset(block, 0, bytes);
  return phys_frame_from_addr((phys_addr_t)(uintptr_t)block);
}

static void fake_free(phys_frame_t base_frame, size_t page_count) {
  (void)page_count;
  if(!phys_frame_is_valid(base_frame)) {
    return;
  }
  void* block = (void*)(uintptr_t)phys_frame_to_addr(base_frame);
  free(block);
}

void setUp(void) {
  shm_manager_initialize();
  shm_set_allocator(fake_alloc, fake_free);
}

void tearDown(void) {
}

void test_shm_region_create_assigns_id(void) {
  shm_region_create_status_t status;
  shm_region_t* region = shm_region_create(0x42, PAGE_SIZE * 2, 0600, NULL, &status);
  TEST_ASSERT_NOT_NULL(region);
  TEST_ASSERT_EQUAL_INT(SHM_REGION_CREATE_OK, status);
  TEST_ASSERT_GREATER_THAN(0, shm_region_id(region));
  TEST_ASSERT_EQUAL_size_t(PAGE_SIZE * 2, shm_region_size(region));
  TEST_ASSERT_EQUAL_size_t(2, shm_region_page_count(region));
  TEST_ASSERT_EQUAL_INT(0x42, shm_region_key(region));
  TEST_ASSERT_EQUAL_UINT16(0600, shm_region_mode(region));
  shm_region_unref(region);
}

void test_shm_region_get_by_id_returns_same_region(void) {
  shm_region_t* region = shm_region_create(0x10, PAGE_SIZE, 0644, NULL, NULL);
  TEST_ASSERT_NOT_NULL(region);
  int id = shm_region_id(region);

  shm_region_t* fetched = shm_region_get_by_id(id);
  TEST_ASSERT_NOT_NULL(fetched);
  TEST_ASSERT_EQUAL(id, shm_region_id(fetched));
  TEST_ASSERT_EQUAL_size_t(shm_region_page_count(region),
                           shm_region_page_count(fetched));

  shm_region_unref(fetched);
  shm_region_unref(region);
}

void test_shm_region_unref_removes_region_when_no_refs(void) {
  shm_region_t* region = shm_region_create(0x77, PAGE_SIZE, 0600, NULL, NULL);
  TEST_ASSERT_NOT_NULL(region);
  int id = shm_region_id(region);

  shm_region_unref(region);

  shm_region_t* lookup = shm_region_get_by_id(id);
  TEST_ASSERT_NULL(lookup);
}

void test_shm_region_attachment_counter_tracks_updates(void) {
  shm_region_t* region = shm_region_create(123, PAGE_SIZE, 0600, NULL, NULL);
  TEST_ASSERT_NOT_NULL(region);

  TEST_ASSERT_EQUAL_size_t(0, shm_region_attachment_count(region));
  shm_region_increment_attachments(region);
  TEST_ASSERT_EQUAL_size_t(1, shm_region_attachment_count(region));
  shm_region_increment_attachments(region);
  TEST_ASSERT_EQUAL_size_t(2, shm_region_attachment_count(region));
  shm_region_decrement_attachments(region);
  TEST_ASSERT_EQUAL_size_t(1, shm_region_attachment_count(region));

  shm_region_unref(region);
}

void test_shm_region_create_rejects_zero_size(void) {
  shm_region_create_status_t status;
  shm_region_t* region = shm_region_create(0, 0, 0, NULL, &status);
  TEST_ASSERT_NULL(region);
  TEST_ASSERT_EQUAL_INT(SHM_REGION_CREATE_INVALID_ARGUMENT, status);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_shm_region_create_assigns_id);
  RUN_TEST(test_shm_region_get_by_id_returns_same_region);
  RUN_TEST(test_shm_region_unref_removes_region_when_no_refs);
  RUN_TEST(test_shm_region_attachment_counter_tracks_updates);
  RUN_TEST(test_shm_region_create_rejects_zero_size);
  return UNITY_END();
}
