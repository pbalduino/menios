#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unity.h>

#define MOSH_TEST

#define MOSH_CAPTURE_CAPACITY 4096

long mosh_test_syscall0(long number);
long mosh_test_syscall1(long number, long arg1);
long mosh_test_syscall3(long number, long arg1, long arg2, long arg3);
void mosh_test_set_env(char** envp);

static char   g_capture[MOSH_CAPTURE_CAPACITY];
static size_t g_capture_length;

static void reset_capture(void) {
  g_capture_length = 0;
  memset(g_capture, 0, sizeof(g_capture));
}

void mosh_test_write_bytes(int fd, const char* data, size_t length) {
  (void)fd;
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

long mosh_test_syscall0(long number) {
  (void)number;
  return 0;
}

long mosh_test_syscall1(long number, long arg1) {
  (void)number;
  (void)arg1;
  return 0;
}

long mosh_test_syscall2(long number, long arg1, long arg2) {
  (void)number;
  (void)arg1;
  (void)arg2;
  return 0;
}

long mosh_test_syscall3(long number, long arg1, long arg2, long arg3) {
  (void)number;
  (void)arg1;
  (void)arg2;
  (void)arg3;
  return 0;
}

static int capture_contains(const char* text) {
  size_t needle_len = strlen(text);
  if(needle_len == 0 || g_capture_length < needle_len) {
    return 0;
  }
  for(size_t i = 0; i <= g_capture_length - needle_len; i++) {
    if(memcmp(&g_capture[i], text, needle_len) == 0) {
      return 1;
    }
  }
  return 0;
}

void setUp(void) {
  reset_capture();
  mosh_test_set_env(NULL);
}

void tearDown(void) {}

void test_default_environment_provides_path(void);
void test_pwd_builtin_prints_path(void);
void test_echo_builtin_emits_arguments(void);

void test_backspace_redraws_with_carriage_return(void) {
  line_state_t state;
  char buffer[32];

  line_state_init(&state, MOSH_PROMPT, buffer, sizeof(buffer));
  reset_capture();

  line_insert_char(&state, 'h');
  line_insert_char(&state, 'i');
  reset_capture();

  line_backspace(&state);

  TEST_ASSERT_TRUE_MESSAGE(capture_contains("\r" MOSH_PROMPT),
                           "Backspace should redraw prompt starting with carriage return");
}

void test_delete_at_cursor_updates_buffer(void) {
  line_state_t state;
  char buffer[32];

  line_state_init(&state, MOSH_PROMPT, buffer, sizeof(buffer));
  line_insert_char(&state, 'a');
  line_insert_char(&state, 'b');
  line_insert_char(&state, 'c');
  line_cursor_left(&state);

  line_delete_at_cursor(&state);

  TEST_ASSERT_EQUAL_STRING("ab", state.buffer);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(2, state.length, "Delete should shrink line length");
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(2, state.cursor, "Delete keeps cursor at deletion point");
}

void test_history_navigation_restores_last_entry(void) {
  history_reset();
  history_add("first");
  history_add("second");

  line_state_t state;
  char buffer[32];

  line_state_init(&state, MOSH_PROMPT, buffer, sizeof(buffer));

  history_load_into(&state, history_length - 1);

  TEST_ASSERT_EQUAL_STRING("second", state.buffer);
  TEST_ASSERT_EQUAL_UINT64(strlen("second"), state.length);
  TEST_ASSERT_EQUAL_UINT64(state.length, state.cursor);
}

void test_hide_caret_restores_character(void) {
  line_state_t state;
  char buffer[32];

  line_state_init(&state, MOSH_PROMPT, buffer, sizeof(buffer));
  line_insert_char(&state, 'x');
  line_cursor_left(&state);
  state.caret_visible = true;
  reset_capture();

  line_hide_caret(&state);

  TEST_ASSERT_FALSE(state.caret_visible);
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("x"),
                           "Hiding caret should rewrite underlying character");
}

void test_default_environment_provides_path(void) {
  mosh_test_set_env(NULL);
  TEST_ASSERT_EQUAL_STRING("/", env_get("PWD"));
  TEST_ASSERT_EQUAL_STRING("/", env_get("HOME"));
  TEST_ASSERT_EQUAL_STRING("/bin", env_get("PATH"));
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_backspace_redraws_with_carriage_return);
  RUN_TEST(test_delete_at_cursor_updates_buffer);
  RUN_TEST(test_history_navigation_restores_last_entry);
  RUN_TEST(test_hide_caret_restores_character);
  RUN_TEST(test_default_environment_provides_path);
  RUN_TEST(test_pwd_builtin_prints_path);
  RUN_TEST(test_echo_builtin_emits_arguments);

  return UNITY_END();
}

void test_pwd_builtin_prints_path(void) {
  static char pwd_entry[64] = "PWD=/";
  static char* envp[] = { pwd_entry, NULL };
  mosh_test_set_env(envp);
  TEST_ASSERT_TRUE(handle_builtin("cd /system"));
  reset_capture();
  TEST_ASSERT_TRUE(handle_builtin("pwd"));
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("/system\n"),
                           "pwd builtin should print PWD env value");
}

void test_echo_builtin_emits_arguments(void) {
  reset_capture();
  TEST_ASSERT_TRUE(handle_builtin("echo hello world"));
  TEST_ASSERT_TRUE_MESSAGE(capture_contains("hello world\n"),
                           "echo builtin should print arguments");
}
