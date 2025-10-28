#include "unity.h"

#include <sys/fcntl.h>
#include <sys/stat.h>

#include <kernel/devfs.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/procfs.h>
#include <kernel/vfs.h>

void setUp(void) {
  vfs_shutdown();
}

void tearDown(void) {
  vfs_shutdown();
}

static void assert_directory(const fs_path_info_t* info) {
  TEST_ASSERT_NOT_NULL(info);
  TEST_ASSERT_TRUE(info->is_directory);
  TEST_ASSERT_TRUE(info->has_mode);
  TEST_ASSERT_EQUAL_INT(S_IFDIR, info->mode & S_IFMT);
}

static void assert_regular_file(const fs_path_info_t* info) {
  TEST_ASSERT_NOT_NULL(info);
  TEST_ASSERT_FALSE(info->is_directory);
  TEST_ASSERT_TRUE(info->has_mode);
  TEST_ASSERT_EQUAL_INT(S_IFREG, info->mode & S_IFMT);
}

void test_procfs_stat_reports_entries(void) {
  TEST_ASSERT_TRUE(procfs_mount());

  fs_path_info_t info;
  TEST_ASSERT_TRUE(vfs_path_info("/proc", &info));
  assert_directory(&info);

  TEST_ASSERT_TRUE(vfs_path_info("/proc/meminfo", &info));
  assert_regular_file(&info);
  TEST_ASSERT_TRUE(info.is_read_only);
  TEST_ASSERT_GREATER_THAN_UINT64(0u, info.size);
}

void test_devfs_stat_reports_char_devices(void) {
  TEST_ASSERT_TRUE(devfs_mount());

  fs_path_info_t info;
  TEST_ASSERT_TRUE(vfs_path_info("/dev", &info));
  assert_directory(&info);

  TEST_ASSERT_TRUE(vfs_path_info("/dev/null", &info));
  TEST_ASSERT_FALSE(info.is_directory);
  TEST_ASSERT_TRUE(info.has_mode);
  TEST_ASSERT_EQUAL_INT(S_IFCHR, info.mode & S_IFMT);

  file_t* file = NULL;
  int rc = vfs_open("/dev/null", O_WRONLY, &file);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_NOT_NULL(file);

  struct stat st;
  TEST_ASSERT_NOT_NULL(file->ops);
  TEST_ASSERT_NOT_NULL(file->ops->stat);
  TEST_ASSERT_EQUAL_INT(0, file->ops->stat(file, &st));
  TEST_ASSERT_TRUE(S_ISCHR(st.st_mode));

  file_unref(file);
}

void test_pipe_stat_reports_fifo(void) {
  file_t* read_end = NULL;
  file_t* write_end = NULL;
  TEST_ASSERT_EQUAL_INT(0, pipe_create(&read_end, &write_end));
  TEST_ASSERT_NOT_NULL(read_end);
  TEST_ASSERT_NOT_NULL(write_end);

  struct stat st;
  TEST_ASSERT_NOT_NULL(read_end->ops);
  TEST_ASSERT_NOT_NULL(read_end->ops->stat);
  TEST_ASSERT_EQUAL_INT(0, read_end->ops->stat(read_end, &st));
  TEST_ASSERT_TRUE(S_ISFIFO(st.st_mode));
  TEST_ASSERT_EQUAL_INT(4096, st.st_blksize);

  TEST_ASSERT_NOT_NULL(write_end->ops);
  TEST_ASSERT_NOT_NULL(write_end->ops->stat);
  TEST_ASSERT_EQUAL_INT(0, write_end->ops->stat(write_end, &st));
  TEST_ASSERT_TRUE(S_ISFIFO(st.st_mode));

  file_unref(read_end);
  file_unref(write_end);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_procfs_stat_reports_entries);
  RUN_TEST(test_devfs_stat_reports_char_devices);
  RUN_TEST(test_pipe_stat_reports_fifo);
  return UNITY_END();
}
