#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>
#include <menios/syscall.h>

static char ps_output[1024];
static size_t ps_output_len;
static const char* mock_proc_list;

static int test_fputs(const char* s, FILE* stream) {
  (void)stream;
  size_t len = strlen(s);
  if(len > sizeof(ps_output) - ps_output_len - 1) {
    len = sizeof(ps_output) - ps_output_len - 1;
  }
  memcpy(&ps_output[ps_output_len], s, len);
  ps_output_len += len;
  ps_output[ps_output_len] = '\0';
  return 0;
}

long __menios_syscall2(long number, long arg1, long arg2) {
  TEST_ASSERT_EQUAL_MESSAGE(SYS_PROC_LIST, number, "ps should call SYS_PROC_LIST");
  char* buffer = (char*)arg1;
  size_t capacity = (size_t)arg2;
  size_t len = strlen(mock_proc_list);
  size_t copy = len;
  if(copy > capacity) {
    copy = capacity;
  }
  memcpy(buffer, mock_proc_list, copy);
  return (long)len;
}

#define fputs test_fputs
#define main ps_main
#include "../app/ps/ps.c"
#undef main
#undef fputs

void setUp(void) {
  ps_output_len = 0;
  ps_output[0] = '\0';
}

void tearDown(void) {}

void test_ps_prints_proc_list(void) {
  mock_proc_list = "PID STATE NAME\n1 running init\n";
  int rc = ps_main();
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_STRING(mock_proc_list, ps_output);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ps_prints_proc_list);
  return UNITY_END();
}
