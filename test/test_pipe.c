#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <unity.h>

#include <kernel/file.h>
#include <kernel/proc.h>

static void reset_current(void) {
  current = NULL;
  memset(&kernel_process_info, 0, sizeof(kernel_process_info));
}

void setUp(void) {}
void tearDown(void) {}

void test_pipe_roundtrip(void) {
  file_t* reader = NULL;
  file_t* writer = NULL;
  TEST_ASSERT_EQUAL_INT(0, pipe_create(&reader, &writer));
  TEST_ASSERT_NOT_NULL(reader);
  TEST_ASSERT_NOT_NULL(writer);

  const char message[] = "hello pipe";
  int64_t written = file_write(writer, message, strlen(message));
  TEST_ASSERT_EQUAL_INT((int)strlen(message), (int)written);

  char buffer[32];
  memset(buffer, 0, sizeof(buffer));
  int64_t read = file_read(reader, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_INT((int)strlen(message), (int)read);
  TEST_ASSERT_EQUAL_STRING(message, buffer);

  /* Closing writer should signal EOF to reader. */
  file_unref(writer);
  writer = NULL;

  memset(buffer, 0, sizeof(buffer));
  read = file_read(reader, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_INT(0, (int)read);

  file_unref(reader);
}

void test_pipe_write_after_reader_closed_returns_epipe(void) {
  file_t* reader = NULL;
  file_t* writer = NULL;
  TEST_ASSERT_EQUAL_INT(0, pipe_create(&reader, &writer));

  file_unref(reader);
  reader = NULL;

  int64_t rc = file_write(writer, "x", 1);
  TEST_ASSERT_EQUAL_INT(-EPIPE, (int)rc);

  file_unref(writer);
}

void test_pipe_syscall_roundtrip(void) {
  reset_current();
  current = &kernel_process_info;
  proc_file_table_initialize(current);

  int fds[2] = { -1, -1 };
  TEST_ASSERT_EQUAL_INT(0, pipe(fds));
  TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fds[0]);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fds[1]);

  const char message[] = "syscall pipe";
  ssize_t written = write(fds[1], message, strlen(message));
  TEST_ASSERT_EQUAL_INT((int)strlen(message), (int)written);

  char buffer[32];
  memset(buffer, 0, sizeof(buffer));
  ssize_t read_len = read(fds[0], buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_INT((int)strlen(message), (int)read_len);
  TEST_ASSERT_EQUAL_STRING(message, buffer);

  TEST_ASSERT_EQUAL_INT(0, close(fds[0]));
  TEST_ASSERT_EQUAL_INT(0, close(fds[1]));

  proc_file_table_cleanup(current);
  reset_current();
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pipe_roundtrip);
  RUN_TEST(test_pipe_write_after_reader_closed_returns_epipe);
   RUN_TEST(test_pipe_syscall_roundtrip);
  return UNITY_END();
}
