#include "unity.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <kernel/file.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>

typedef struct fake_file_entry_t {
  const char* path;
  const char* contents;
} fake_file_entry_t;

static const fake_file_entry_t fake_files[] = {
  { "/present.txt", "hello" },
  { NULL, NULL }
};

static bool fake_path_matches(const char* candidate, const char* requested) {
  if(candidate == NULL || requested == NULL) {
    return false;
  }
  if(strcmp(candidate, requested) == 0) {
    return true;
  }
  if(candidate[0] == '/' && strcmp(candidate + 1, requested) == 0) {
    return true;
  }
  if(requested[0] == '/' && strcmp(requested + 1, candidate) == 0) {
    return true;
  }
  return false;
}

static bool fake_read_all(void* fs_ctx, const char* path, void** out_buffer, size_t* out_size) {
  (void)fs_ctx;
  if(path == NULL || out_buffer == NULL || out_size == NULL) {
    return false;
  }

  for(size_t idx = 0; fake_files[idx].path != NULL; ++idx) {
    if(fake_path_matches(fake_files[idx].path, path)) {
      size_t len = strlen(fake_files[idx].contents);
      uint8_t* buffer = kmalloc(len);
      if(buffer == NULL) {
        return false;
      }
      memcpy(buffer, fake_files[idx].contents, len);
      *out_buffer = buffer;
      *out_size = len;
      return true;
    }
  }

  return false;
}

static bool fake_list(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context) {
  (void)fs_ctx;
  (void)path;
  (void)iter;
  (void)context;
  return false;
}

static const vfs_fs_driver_t fake_driver = {
  .list = fake_list,
  .read = NULL,
  .read_all = fake_read_all,
  .open = NULL,
  .unlink = NULL,
  .destroy = NULL,
};

void setUp(void) {
  vfs_shutdown();
  TEST_ASSERT_TRUE(vfs_mount_root(&fake_driver, NULL, true));
}

void tearDown(void) {
  vfs_shutdown();
}

void test_vfs_open_readonly_success(void) {
  file_t* file = NULL;
  int rc = vfs_open("/present.txt", O_RDONLY, &file);
  TEST_ASSERT_EQUAL(0, rc);
  TEST_ASSERT_NOT_NULL(file);

  uint8_t buffer[8] = {0};
  int64_t bytes = file->ops->read(file, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_INT64(5, bytes);
  TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t*)"hello", buffer, (size_t)bytes);

  int64_t position = file->ops->seek(file, 0, SEEK_CUR);
  TEST_ASSERT_EQUAL_INT64(bytes, position);

  file_unref(file);
}

void test_vfs_open_missing_returns_enoent(void) {
  file_t* file = NULL;
  int rc = vfs_open("/missing.txt", O_RDONLY, &file);
  TEST_ASSERT_EQUAL_INT(-ENOENT, rc);
  TEST_ASSERT_NULL(file);
}

void test_vfs_open_rejects_write_flags(void) {
  file_t* file = NULL;
  int rc = vfs_open("/present.txt", O_WRONLY, &file);
  TEST_ASSERT_EQUAL_INT(-EROFS, rc);
  TEST_ASSERT_NULL(file);

  rc = vfs_open("/present.txt", O_RDWR | O_APPEND, &file);
  TEST_ASSERT_EQUAL_INT(-EROFS, rc);
  TEST_ASSERT_NULL(file);
}

void test_vfs_open_rejects_directory_flag(void) {
  file_t* file = NULL;
  int rc = vfs_open("/present.txt", O_RDONLY | O_DIRECTORY, &file);
  TEST_ASSERT_EQUAL_INT(-ENOSYS, rc);
  TEST_ASSERT_NULL(file);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_vfs_open_readonly_success);
  RUN_TEST(test_vfs_open_missing_returns_enoent);
  RUN_TEST(test_vfs_open_rejects_write_flags);
  RUN_TEST(test_vfs_open_rejects_directory_flag);
  return UNITY_END();
}
