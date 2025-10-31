#include "unity.h"

#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include <kernel/file.h>
#include <kernel/fs/devfs/devfs.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/core.h>

static int test_char_open(char_device_t* device, int flags, file_t** out_file) {
  (void)device;
  (void)flags;
  (void)out_file;
  return -ENOSYS;
}

static char_device_t make_test_device(const char* name, uint32_t mode, unsigned int minors) {
  char_device_t device = {
    .name = name,
    .access_mode = mode,
    .open = test_char_open,
    .driver_data = NULL,
    .dev = 0,
    .minor_count = minors,
  };
  return device;
}

void setUp(void) {
  char_device_system_init();
}

void tearDown(void) {
  vfs_shutdown();
}

void test_char_device_register_rejects_null(void) {
  TEST_ASSERT_EQUAL_INT(-EINVAL, char_device_register(NULL));
}

void test_char_device_register_rejects_missing_access(void) {
  char_device_t device = make_test_device("testdev_none", 0u, 1u);
  TEST_ASSERT_EQUAL_INT(-EINVAL, char_device_register(&device));
}

void test_char_device_register_dynamic_assigns_reserved_major(void) {
  char_device_t device = make_test_device("testdev_dyn0", FILE_MODE_READ, 1u);
  TEST_ASSERT_EQUAL_INT(0, char_device_register(&device));
  unsigned int assigned_major = MAJOR(device.dev);
  TEST_ASSERT_TRUE(assigned_major >= 10u);

  char_device_unregister(&device);
}

void test_char_device_register_minor_range_lookup(void) {
  char_device_t device = make_test_device("testdev_range", FILE_MODE_READ, 4u);
  TEST_ASSERT_EQUAL_INT(0, char_device_register(&device));

  unsigned int major = MAJOR(device.dev);
  unsigned int base_minor = MINOR(device.dev);

  for(unsigned int offset = 0; offset < device.minor_count; ++offset) {
    dev_t candidate = MKDEV(major, base_minor + offset);
    TEST_ASSERT_EQUAL_PTR(&device, char_device_lookup(candidate));
  }

  dev_t out_of_range = MKDEV(major, base_minor + device.minor_count);
  TEST_ASSERT_NULL(char_device_lookup(out_of_range));

  char_device_unregister(&device);
}

void test_char_device_unregister_removes_lookup_entry(void) {
  char_device_t device = make_test_device("testdev_unregister", FILE_MODE_READ, 1u);
  TEST_ASSERT_EQUAL_INT(0, char_device_register(&device));

  TEST_ASSERT_EQUAL_PTR(&device, char_device_lookup(device.dev));
  char_device_unregister(&device);
  TEST_ASSERT_NULL(char_device_lookup(device.dev));
}

void test_char_device_register_name_conflict(void) {
  char_device_t first = make_test_device("testdev_conflict", FILE_MODE_READ, 1u);
  char_device_t second = make_test_device("testdev_conflict", FILE_MODE_READ, 1u);

  TEST_ASSERT_EQUAL_INT(0, char_device_register(&first));
  TEST_ASSERT_EQUAL_INT(-EEXIST, char_device_register(&second));

  char_device_unregister(&first);
}

void test_char_device_register_range_conflict(void) {
  char_device_t first = make_test_device("testdev_range1", FILE_MODE_READ, 2u);
  first.dev = MKDEV(20u, 0u);
  TEST_ASSERT_EQUAL_INT(0, char_device_register(&first));

  char_device_t overlap = make_test_device("testdev_overlap", FILE_MODE_READ, 1u);
  overlap.dev = MKDEV(20u, 1u);
  TEST_ASSERT_EQUAL_INT(-EEXIST, char_device_register(&overlap));

  char_device_unregister(&first);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_char_device_register_rejects_null);
  RUN_TEST(test_char_device_register_rejects_missing_access);
  RUN_TEST(test_char_device_register_dynamic_assigns_reserved_major);
  RUN_TEST(test_char_device_register_minor_range_lookup);
  RUN_TEST(test_char_device_unregister_removes_lookup_entry);
  RUN_TEST(test_char_device_register_name_conflict);
  RUN_TEST(test_char_device_register_range_conflict);
  return UNITY_END();
}
