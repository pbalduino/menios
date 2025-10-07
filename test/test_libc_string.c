#include <stdio.h>
#include <string.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_memcpy_copies_bytes(void) {
  char src[] = "hello";
  char dest[sizeof(src)];
  memcpy(dest, src, sizeof(src));
  TEST_ASSERT_EQUAL_STRING("hello", dest);
}

void test_memmove_handles_overlap(void) {
  char buffer[8] = "abcdef";
  memmove(buffer + 1, buffer, 5);
  buffer[6] = '\0';
  TEST_ASSERT_EQUAL_STRING("aabcde", buffer);
}

void test_memset_fills_buffer(void) {
  char buffer[5];
  memset(buffer, 'x', 4);
  buffer[4] = '\0';
  TEST_ASSERT_EQUAL_STRING("xxxx", buffer);
}

void test_memcmp_detects_difference(void) {
  const char lhs[] = { 'a', 'b', 'c' };
  const char rhs[] = { 'a', 'b', 'd' };
  TEST_ASSERT_LESS_THAN(0, memcmp(lhs, rhs, sizeof(lhs)));
  TEST_ASSERT_GREATER_THAN(0, memcmp(rhs, lhs, sizeof(lhs)));
  TEST_ASSERT_EQUAL_INT(0, memcmp(lhs, lhs, sizeof(lhs)));
}

void test_memchr_finds_value(void) {
  const char buffer[] = "hello";
  TEST_ASSERT_EQUAL_PTR(&buffer[1], memchr(buffer, 'e', sizeof(buffer)));
  TEST_ASSERT_NULL(memchr(buffer, 'z', sizeof(buffer)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_memcpy_copies_bytes);
  RUN_TEST(test_memmove_handles_overlap);
  RUN_TEST(test_memset_fills_buffer);
  RUN_TEST(test_memcmp_detects_difference);
  RUN_TEST(test_memchr_finds_value);
  return UNITY_END();
}
