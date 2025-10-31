#include "unity.h"

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include <kernel/file.h>
#include <kernel/proc.h>
#include <kernel/syscall.h>

extern proc_info_p current;

static proc_info_t proc_state;

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

static int install_tty_fd(void) {
  file_t* tty = file_create_tty_console_file();
  TEST_ASSERT_NOT_NULL(tty);
  int fd = proc_file_install(&proc_state, tty, 0);
  file_unref(tty);
  TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
  return fd;
}

void setUp(void) {
  memset(&proc_state, 0, sizeof(proc_state));
  proc_file_table_init(&proc_state);
  current = &proc_state;
  syscall_initialize();
}

void tearDown(void) {
  proc_file_table_cleanup(&proc_state);
  current = NULL;
}

void test_ioctl_tty_get_winsize_returns_default(void) {
  int fd = install_tty_fd();

  struct winsize ws;
  memset(&ws, 0, sizeof(ws));

  uint64_t rc = dispatch_syscall(SYS_IOCTL, (uint64_t)fd, (uint64_t)TIOCGWINSZ, (uint64_t)&ws);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_UINT16(25, ws.ws_row);
  TEST_ASSERT_EQUAL_UINT16(80, ws.ws_col);
}

void test_ioctl_tty_unknown_command_returns_enotty(void) {
  int fd = install_tty_fd();

  uint64_t rc = dispatch_syscall(SYS_IOCTL, (uint64_t)fd, (uint64_t)_IO('x', 0), 0);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)(-ENOTTY), rc);
}

void test_ioctl_invalid_fd_returns_ebadf(void) {
  uint64_t rc = dispatch_syscall(SYS_IOCTL, 99u, 0, 0);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)(-EBADF), rc);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ioctl_tty_get_winsize_returns_default);
  RUN_TEST(test_ioctl_tty_unknown_command_returns_enotty);
  RUN_TEST(test_ioctl_invalid_fd_returns_ebadf);
  return UNITY_END();
}
