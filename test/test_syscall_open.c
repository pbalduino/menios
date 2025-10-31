#include "unity.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <kernel/file.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/syscall.h>
#include <kernel/fs/vfs/vfs.h>

extern proc_info_p current;

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
  .write = NULL,
  .write_all = NULL,
  .create_file = NULL,
  .truncate_file = NULL,
  .stat = NULL,
  .open = NULL,
  .unlink = NULL,
  .mkdir = NULL,
  .rmdir = NULL,
  .rename = NULL,
  .chmod = NULL,
  .utimens = NULL,
  .destroy = NULL,
};

static proc_info_t proc_state;
static cpu_state_t cpu_state;

void setUp(void) {
  memset(&proc_state, 0, sizeof(proc_state));
  proc_file_table_init(&proc_state);
  proc_state.cwd[0] = '/';
  proc_state.cwd[1] = '\0';
  proc_state.cwd_len = 1;
  proc_state.user_mode = true;
  proc_state.cpu_state = &cpu_state;
  current = &proc_state;
  vfs_shutdown();
  TEST_ASSERT_TRUE(vfs_mount_root(&fake_driver, NULL, true));
  syscall_initialize();
}

void tearDown(void) {
  proc_file_table_cleanup(&proc_state);
  current = NULL;
  vfs_shutdown();
}

static uint64_t dispatch_syscall(uint64_t number,
                                 uint64_t arg0,
                                 uint64_t arg1,
                                 uint64_t arg2) {
  syscall_frame_t frame = {
    .rax = number,
    .rdi = arg0,
    .rsi = arg1,
    .rdx = arg2,
  };
  syscall_dispatch(&frame);
  return frame.rax;
}

void test_syscall_open_read_close_success(void) {
  const char* path = "/present.txt";
  uint64_t rc = dispatch_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)O_RDONLY, 0);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  int fd = (int)rc;

  uint8_t buffer[8] = {0};
  syscall_frame_t read_frame = {
    .rax = SYS_READ,
    .rdi = (uint64_t)fd,
    .rsi = (uint64_t)buffer,
    .rdx = sizeof(buffer),
  };
  syscall_dispatch(&read_frame);
  TEST_ASSERT_EQUAL_INT64(5, (int64_t)read_frame.rax);
  TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t*)"hello", buffer, 5);

  syscall_frame_t seek_frame = {
    .rax = SYS_LSEEK,
    .rdi = (uint64_t)fd,
    .rsi = 0,
    .rdx = SEEK_SET,
  };
  syscall_dispatch(&seek_frame);
  TEST_ASSERT_EQUAL_INT64(0, (int64_t)seek_frame.rax);

  uint64_t close_rc = dispatch_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0);
  TEST_ASSERT_EQUAL_UINT64(0, close_rc);
  TEST_ASSERT_NULL(proc_state.files[fd].file);
}

void test_syscall_open_missing_path(void) {
  const char* path = "/missing.txt";
  uint64_t rc = dispatch_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)O_RDONLY, 0);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)(-ENOENT), rc);
}

void test_syscall_open_rejects_write_flags(void) {
  const char* path = "/present.txt";
  uint64_t rc = dispatch_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)O_WRONLY, 0);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)(-EROFS), rc);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_syscall_open_read_close_success);
  RUN_TEST(test_syscall_open_missing_path);
  RUN_TEST(test_syscall_open_rejects_write_flags);
  return UNITY_END();
}
