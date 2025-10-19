#include "unity.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>

#include <kernel/file.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>

typedef struct fake_file_t {
  const char* path;
  uint8_t*    data;
  size_t      size;
  bool        exists;
  bool        fail_create;
  bool        fail_truncate;
} fake_file_t;

static fake_file_t fake_files[] = {
  { "/existing.txt", NULL, 0, true, false, false },
  { "/append.txt", NULL, 0, true, false, false },
  { "/new.txt", NULL, 0, false, false, false },
  { "/fail_create.txt", NULL, 0, false, true, false },
  { "/fail_truncate.txt", NULL, 0, true, false, true },
  { NULL, NULL, 0, false, false, false }
};

static fake_file_t* find_entry(const char* path) {
  for(fake_file_t* entry = fake_files; entry->path != NULL; ++entry) {
    if(strcmp(entry->path, path) == 0) {
      return entry;
    }
    if(path[0] == '/' && strcmp(entry->path + 1, path + 1) == 0) {
      return entry;
    }
  }
  return NULL;
}

static bool fake_read_all(void* ctx, const char* path, void** out_buffer, size_t* out_size) {
  (void)ctx;
  if(path == NULL || out_buffer == NULL || out_size == NULL) {
    return false;
  }

  fake_file_t* entry = find_entry(path);
  if(entry == NULL || !entry->exists) {
    return false;
  }

  size_t alloc_size = entry->size > 0 ? entry->size : 1u;
  uint8_t* buffer = kmalloc(alloc_size);
  TEST_ASSERT_NOT_NULL(buffer);
  if(entry->size > 0 && entry->data != NULL) {
    memcpy(buffer, entry->data, entry->size);
  } else {
    memset(buffer, 0, alloc_size);
  }

  *out_buffer = buffer;
  *out_size = entry->size;
  return true;
}

static bool fake_read(void* ctx, const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read) {
  (void)ctx;
  if(buffer == NULL) {
    return false;
  }
  fake_file_t* entry = find_entry(path);
  if(entry == NULL || !entry->exists) {
    return false;
  }
  if(offset >= entry->size) {
    if(bytes_read) {
      *bytes_read = 0;
    }
    return true;
  }
  size_t available = entry->size - offset;
  size_t to_copy = length < available ? length : available;
  if(to_copy > 0 && entry->data != NULL) {
    memcpy(buffer, entry->data + offset, to_copy);
  }
  if(bytes_read) {
    *bytes_read = to_copy;
  }
  return true;
}

static bool fake_write(void* ctx, const char* path, size_t offset, const void* buffer, size_t length, size_t* bytes_written) {
  (void)ctx;
  fake_file_t* entry = find_entry(path);
  if(entry == NULL || !entry->exists) {
    return false;
  }

  size_t end = offset + length;
  if(end > entry->size) {
    uint8_t* new_data = kmalloc(end);
    TEST_ASSERT_NOT_NULL(new_data);
    if(entry->data && entry->size > 0) {
      memcpy(new_data, entry->data, entry->size);
    }
    if(entry->data) {
      kfree(entry->data);
    }
    if(entry->size < end) {
      memset(new_data + entry->size, 0, end - entry->size);
    }
    entry->data = new_data;
    entry->size = end;
  }

  if(length > 0) {
    memcpy(entry->data + offset, buffer, length);
  }

  if(bytes_written) {
    *bytes_written = length;
  }
  return true;
}

static bool fake_write_all(void* ctx, const char* path, const void* buffer, size_t size) {
  (void)ctx;
  fake_file_t* entry = find_entry(path);
  if(entry == NULL || !entry->exists) {
    return false;
  }

  if(entry->data) {
    kfree(entry->data);
    entry->data = NULL;
  }

  if(size > 0) {
    entry->data = kmalloc(size);
    TEST_ASSERT_NOT_NULL(entry->data);
    memcpy(entry->data, buffer, size);
  }

  entry->size = size;
  return true;
}

static bool fake_stat(void* ctx, const char* path, size_t* out_size) {
  (void)ctx;
  if(out_size == NULL) {
    return false;
  }
  fake_file_t* entry = find_entry(path);
  if(entry == NULL || !entry->exists) {
    return false;
  }
  *out_size = entry->size;
  return true;
}

static bool fake_create_file(void* ctx, const char* path, bool exclusive) {
  (void)ctx;
  fake_file_t* entry = find_entry(path);
  if(entry == NULL) {
    return false;
  }

  if(entry->fail_create) {
    return false;
  }

  if(entry->exists) {
    return !exclusive;
  }

  entry->exists = true;
  entry->size = 0;
  if(entry->data) {
    kfree(entry->data);
    entry->data = NULL;
  }
  return true;
}

static bool fake_truncate_file(void* ctx, const char* path) {
  (void)ctx;
  fake_file_t* entry = find_entry(path);
  if(entry == NULL || !entry->exists) {
    return false;
  }

  if(entry->fail_truncate) {
    return false;
  }

  entry->size = 0;
  if(entry->data) {
    kfree(entry->data);
    entry->data = NULL;
  }
  return true;
}

static const vfs_fs_driver_t fake_driver = {
  .list = NULL,
  .read = fake_read,
  .read_all = NULL,
  .write = fake_write,
  .write_all = fake_write_all,
  .create_file = fake_create_file,
  .truncate_file = fake_truncate_file,
  .stat = fake_stat,
  .open = NULL,
  .unlink = NULL,
  .mkdir = NULL,
  .rmdir = NULL,
  .rename = NULL,
  .destroy = NULL,
};

static void reset_entries(void) {
  for(fake_file_t* entry = fake_files; entry->path != NULL; ++entry) {
    if(entry->data) {
      kfree(entry->data);
      entry->data = NULL;
    }
    if(strcmp(entry->path, "/existing.txt") == 0) {
      entry->exists = true;
      entry->size = 5;
      entry->data = kmalloc(entry->size);
      TEST_ASSERT_NOT_NULL(entry->data);
      memcpy(entry->data, "hello", entry->size);
    } else if(strcmp(entry->path, "/append.txt") == 0) {
      entry->exists = true;
      entry->size = 4;
      entry->data = kmalloc(entry->size);
      TEST_ASSERT_NOT_NULL(entry->data);
      memcpy(entry->data, "abcd", entry->size);
    } else if(strcmp(entry->path, "/fail_truncate.txt") == 0) {
      entry->exists = true;
      entry->size = 3;
      entry->data = kmalloc(entry->size);
      TEST_ASSERT_NOT_NULL(entry->data);
      memcpy(entry->data, "xyz", entry->size);
    } else {
      entry->exists = false;
      entry->size = 0;
    }
  }
}

void setUp(void) {
  vfs_shutdown();
  reset_entries();
  TEST_ASSERT_TRUE(vfs_mount_root(&fake_driver, NULL, false));
}

void tearDown(void) {
  vfs_shutdown();
  for(fake_file_t* entry = fake_files; entry->path != NULL; ++entry) {
    if(entry->data) {
      kfree(entry->data);
      entry->data = NULL;
    }
  }
}

void test_vfs_open_creates_new_file(void) {
  fake_file_t* entry = find_entry("/new.txt");
  TEST_ASSERT_NOT_NULL(entry);
  TEST_ASSERT_FALSE(entry->exists);

  file_t* file = NULL;
  int rc = vfs_open("/new.txt", O_CREAT | O_WRONLY, &file);
  TEST_ASSERT_EQUAL(0, rc);
  TEST_ASSERT_NOT_NULL(file);
  TEST_ASSERT_TRUE(entry->exists);
  TEST_ASSERT_EQUAL_size_t(0u, entry->size);

  const char payload[] = "data";
  TEST_ASSERT_EQUAL_INT64((int64_t)sizeof(payload) - 1, file->ops->write(file, payload, sizeof(payload) - 1));
  file_unref(file);

  TEST_ASSERT_EQUAL_size_t(sizeof(payload) - 1, entry->size);
  TEST_ASSERT_NOT_NULL(entry->data);
  TEST_ASSERT_EQUAL_MEMORY(payload, entry->data, sizeof(payload) - 1);
}

void test_vfs_open_existing_without_excl_succeeds(void) {
  fake_file_t* entry = find_entry("/existing.txt");
  TEST_ASSERT_NOT_NULL(entry);
  TEST_ASSERT_TRUE(entry->exists);

  file_t* file = NULL;
  int rc = vfs_open("/existing.txt", O_CREAT | O_WRONLY, &file);
  TEST_ASSERT_EQUAL(0, rc);
  TEST_ASSERT_NOT_NULL(file);
  file_unref(file);
}

void test_vfs_open_existing_with_excl_fails(void) {
  file_t* file = NULL;
  int rc = vfs_open("/existing.txt", O_CREAT | O_EXCL | O_WRONLY, &file);
  TEST_ASSERT_EQUAL_INT(-EEXIST, rc);
  TEST_ASSERT_NULL(file);
}

void test_vfs_open_truncates_file(void) {
  fake_file_t* entry = find_entry("/append.txt");
  TEST_ASSERT_NOT_NULL(entry);
  TEST_ASSERT_TRUE(entry->exists);
  TEST_ASSERT_TRUE(entry->size > 0);

  file_t* file = NULL;
  int rc = vfs_open("/append.txt", O_TRUNC | O_WRONLY, &file);
  TEST_ASSERT_EQUAL(0, rc);
  TEST_ASSERT_NOT_NULL(file);
  TEST_ASSERT_EQUAL_size_t(0u, entry->size);

  const char payload[] = "q";
  TEST_ASSERT_EQUAL_INT64(1, file->ops->write(file, payload, 1));
  file_unref(file);

  TEST_ASSERT_EQUAL_size_t(1u, entry->size);
  TEST_ASSERT_EQUAL_UINT8('q', entry->data[0]);
}

void test_vfs_open_create_failure_returns_eio(void) {
  file_t* file = NULL;
  int rc = vfs_open("/fail_create.txt", O_CREAT | O_WRONLY, &file);
  TEST_ASSERT_EQUAL_INT(-EIO, rc);
  TEST_ASSERT_NULL(file);
}

void test_vfs_open_truncate_failure_returns_eio(void) {
  file_t* file = NULL;
  int rc = vfs_open("/fail_truncate.txt", O_TRUNC | O_WRONLY, &file);
  TEST_ASSERT_EQUAL_INT(-EIO, rc);
  TEST_ASSERT_NULL(file);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_vfs_open_creates_new_file);
  RUN_TEST(test_vfs_open_existing_without_excl_succeeds);
  RUN_TEST(test_vfs_open_existing_with_excl_fails);
  RUN_TEST(test_vfs_open_truncates_file);
  RUN_TEST(test_vfs_open_create_failure_returns_eio);
  RUN_TEST(test_vfs_open_truncate_failure_returns_eio);
  return UNITY_END();
}
