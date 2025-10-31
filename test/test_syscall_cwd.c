#include "unity.h"

#include <kernel/file.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/syscall.h>
#include <kernel/fs/vfs/vfs.h>
#include <menios/syscall.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  const char* directories[4];
  const char* files[4];
} stub_fs_t;

static stub_fs_t fs_state = {
  .directories = { "/dir", "/dir/sub", NULL, NULL },
  .files = { "/file", NULL, NULL, NULL },
};

static bool path_matches(const char* lhs, const char* rhs) {
  if(lhs == NULL || rhs == NULL) {
    return false;
  }
  if(lhs[0] == '\0') {
    lhs = "/";
  }
  return strcmp(lhs, rhs) == 0;
}

static bool stub_list(void* ctx, const char* path, vfs_dir_iter_t iter, void* user) {
  (void)iter;
  (void)user;
  stub_fs_t* fs = (stub_fs_t*)ctx;
  if(path == NULL) {
    return false;
  }
  if(path_matches(path, "/") || path_matches(path, "")) {
    return true;
  }
  for(size_t i = 0; i < sizeof(fs->directories) / sizeof(fs->directories[0]); ++i) {
    const char* dir = fs->directories[i];
    if(dir == NULL) {
      continue;
    }
    if(path_matches(path, dir) || (dir[0] == '/' && path_matches(path, dir + 1))) {
      return true;
    }
  }
  return false;
}

static bool stub_read_all(void* ctx, const char* path, void** out_buffer, size_t* out_size) {
  (void)ctx;
  if(path == NULL || out_buffer == NULL || out_size == NULL) {
    return false;
  }
  for(size_t i = 0; i < sizeof(fs_state.files) / sizeof(fs_state.files[0]); ++i) {
    const char* file = fs_state.files[i];
    if(file == NULL) {
      continue;
    }
    if(path_matches(path, file) || (file[0] == '/' && path_matches(path, file + 1))) {
      char* data = kmalloc(1);
      if(data == NULL) {
        return false;
      }
      data[0] = '\0';
      *out_buffer = data;
      *out_size = 1;
      return true;
    }
  }
  return false;
}

static int stub_open(void* ctx, const char* path, int flags, file_t** out_file) {
  (void)ctx;
  (void)flags;
  (void)out_file;
  for(size_t i = 0; i < sizeof(fs_state.files) / sizeof(fs_state.files[0]); ++i) {
    const char* file = fs_state.files[i];
    if(file == NULL) {
      continue;
    }
    if(path_matches(path, file) || (file[0] == '/' && path_matches(path, file + 1))) {
      return -ENOSYS;
    }
  }
  return -ENOENT;
}

static const vfs_fs_driver_t stub_driver = {
  .list = stub_list,
  .read = NULL,
  .read_all = stub_read_all,
  .write = NULL,
  .write_all = NULL,
  .create_file = NULL,
  .truncate_file = NULL,
  .stat = NULL,
  .open = stub_open,
  .unlink = NULL,
  .mkdir = NULL,
  .rmdir = NULL,
  .rename = NULL,
  .chmod = NULL,
  .utimens = NULL,
  .destroy = NULL,
};

static proc_info_t test_proc;
static cpu_state_t cpu_state;

void setUp(void) {
  memset(&test_proc, 0, sizeof(test_proc));
  test_proc.cwd[0] = '/';
  test_proc.cwd[1] = '\0';
  test_proc.cwd_len = 1;
  test_proc.user_mode = true;
  test_proc.cpu_state = &cpu_state;
  current = &test_proc;

  syscall_initialize();
  vfs_initialize();
  bool mounted = vfs_mount("/", &stub_driver, &fs_state, true);
  TEST_ASSERT_TRUE_MESSAGE(mounted, "failed to mount stub fs");
}

void tearDown(void) {
  vfs_shutdown();
}

void test_chdir_relative_updates_cwd(void) {
  syscall_frame_t frame = {
    .rax = SYS_CHDIR,
    .rdi = (uint64_t)"dir",
  };

  uint64_t rc = syscall_dispatch(&frame);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_STRING("/dir", current->cwd);
}

void test_chdir_to_file_returns_enotdir(void) {
  syscall_frame_t frame = {
    .rax = SYS_CHDIR,
    .rdi = (uint64_t)"file",
  };

  uint64_t rc = syscall_dispatch(&frame);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)(-ENOTDIR), rc);
  TEST_ASSERT_EQUAL_STRING("/", current->cwd);
}

void test_chdir_missing_returns_enoent(void) {
  syscall_frame_t frame = {
    .rax = SYS_CHDIR,
    .rdi = (uint64_t)"missing",
  };

  uint64_t rc = syscall_dispatch(&frame);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)(-ENOENT), rc);
  TEST_ASSERT_EQUAL_STRING("/", current->cwd);
}

void test_getcwd_exposes_current_directory(void) {
  syscall_frame_t change = {
    .rax = SYS_CHDIR,
    .rdi = (uint64_t)"dir/sub",
  };
  uint64_t rc = syscall_dispatch(&change);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_STRING("/dir/sub", current->cwd);

  char buffer[64];
  syscall_frame_t frame = {
    .rax = SYS_GETCWD,
    .rdi = (uint64_t)buffer,
    .rsi = sizeof(buffer),
  };

  uint64_t result = syscall_dispatch(&frame);
  TEST_ASSERT_EQUAL_PTR(buffer, (char*)(uintptr_t)result);
  TEST_ASSERT_EQUAL_STRING("/dir/sub", buffer);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_chdir_relative_updates_cwd);
  RUN_TEST(test_chdir_to_file_returns_enotdir);
  RUN_TEST(test_chdir_missing_returns_enoent);
  RUN_TEST(test_getcwd_exposes_current_directory);
  return UNITY_END();
}
