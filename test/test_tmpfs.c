#include <unity.h>

#include <kernel/fs/tmpfs/tmpfs.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/file.h>
#include <sys/fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

static void mount_tmpfs(void) {
  TEST_ASSERT_TRUE(vfs_initialize());
  TEST_ASSERT_TRUE(tmpfs_mount());
}

void test_tmpfs_mount_and_roundtrip(void) {
  mount_tmpfs();

  const char *path = "/tmp/testfile";
  const char *message = "hello tmpfs";

  file_t *file = NULL;
  TEST_ASSERT_EQUAL_INT(0, vfs_open(path, O_CREAT | O_RDWR, &file));
  TEST_ASSERT_NOT_NULL(file);

  int64_t written = file_write(file, message, strlen(message));
  TEST_ASSERT_EQUAL_INT((int)strlen(message), (int)written);

  TEST_ASSERT_EQUAL_INT64(0, file_seek(file, 0, SEEK_SET));

  char buffer[32];
  memset(buffer, 0, sizeof(buffer));
  int64_t read = file_read(file, buffer, strlen(message));
  TEST_ASSERT_EQUAL_INT((int)strlen(message), (int)read);
  buffer[read] = '\0';
  TEST_ASSERT_EQUAL_STRING(message, buffer);

  file_unref(file);

  file = NULL;
  TEST_ASSERT_EQUAL_INT(0, vfs_open(path, O_RDONLY, &file));
  TEST_ASSERT_NOT_NULL(file);

  memset(buffer, 0, sizeof(buffer));
  read = file_read(file, buffer, sizeof(buffer) - 1);
  TEST_ASSERT_GREATER_THAN_INT(0, (int)read);
  buffer[read] = '\0';
  TEST_ASSERT_EQUAL_STRING(message, buffer);

  file_unref(file);

  vfs_shutdown();
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tmpfs_mount_and_roundtrip);
  return UNITY_END();
}
