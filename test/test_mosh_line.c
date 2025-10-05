#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#define MOSH_TEST

#define MOSH_CAPTURE_CAPACITY 4096

static char   g_capture[MOSH_CAPTURE_CAPACITY];
static size_t g_capture_length;

static void mosh_test_reset_output(void) {
  g_capture_length = 0;
}

void mosh_test_write_bytes(int fd, const char* data, size_t length) {
  (void)fd; // All output goes to the capture buffer in tests.
  if(data == NULL || length == 0) {
    return;
  }
  if(length > MOSH_CAPTURE_CAPACITY - g_capture_length) {
    length = MOSH_CAPTURE_CAPACITY - g_capture_length;
  }
  if(length == 0) {
    return;
  }
  memcpy(&g_capture[g_capture_length], data, length);
  g_capture_length += length;
}

#include "../app/mosh/mosh.c"

static int find_subsequence(const char* haystack, size_t hay_len, const char* needle, size_t needle_len) {
  if(needle_len == 0 || hay_len < needle_len) {
    return -1;
  }
  for(size_t i = 0; i <= hay_len - needle_len; i++) {
    if(memcmp(&haystack[i], needle, needle_len) == 0) {
      return (int)i;
    }
  }
  return -1;
}

void setUp(void) {
  mosh_test_reset_output();
}

void tearDown(void) {}

void test_backspace_redraws_with_carriage_return(void) {
  line_state_t state;
  char buffer[32];

  line_state_init(&state, MOSH_PROMPT, buffer, sizeof(buffer));
  mosh_test_reset_output();

  line_insert_char(&state, 'h');
  line_insert_char(&state, 'i');
  mosh_test_reset_output();

  line_backspace(&state);

  const char expected[] = "\r" MOSH_PROMPT;
  TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0,
                                           find_subsequence(g_capture, g_capture_length, expected, sizeof(expected) - 1),
                                           "line_backspace must emit carriage return before refilling the prompt");
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_backspace_redraws_with_carriage_return);

  return UNITY_END();
}
