#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define SYS_READ   0
#define SYS_WRITE  1
#define SYS_CLOSE  3
#define SYS_DUP2    33
#define SYS_SLEEP   35
#define SYS_FORK    57
#define SYS_EXECVE  59
#define SYS_EXIT    60
#define SYS_WAITPID 61

#define STDIN_FILENO   0
#define STDOUT_FILENO  1
#define STDERR_FILENO  2

#define MOSH_MAX_LINE_LENGTH 256
#define MOSH_HISTORY_LIMIT   16
#define MOSH_PROMPT          "mosh:/>"

static char** process_envp = NULL;
static char*  fallback_envp[] = { NULL };
static const char DEFAULT_PATH[] = "/bin";

#ifdef MOSH_TEST
long mosh_test_syscall0(long number);
long mosh_test_syscall1(long number, long arg1);
long mosh_test_syscall3(long number, long arg1, long arg2, long arg3);

static inline long syscall0(long number) {
  return mosh_test_syscall0(number);
}

static inline long syscall1(long number, long arg1) {
  return mosh_test_syscall1(number, arg1);
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
  return mosh_test_syscall3(number, arg1, arg2, arg3);
}
#else
static inline long syscall0(long number) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(number) : "rcx", "r11", "memory");
  return ret;
}

static inline long syscall1(long number, long arg1) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(number), "D"(arg1) : "rcx", "r11", "memory");
  return ret;
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret)
               : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
               : "rcx", "r11", "memory");
  return ret;
}
#endif

static const char* env_get(const char* key) {
  if(process_envp == NULL || key == NULL) {
    return NULL;
  }

  size_t key_len = str_len(key);
  if(key_len == 0) {
    return NULL;
  }

  for(size_t idx = 0; process_envp[idx] != NULL; idx++) {
    const char* entry = process_envp[idx];
    size_t pos = 0;
    while(entry[pos] != '\0' && entry[pos] != '=') {
      pos++;
    }
    if(entry[pos] != '=' || pos != key_len) {
      continue;
    }

    bool match = true;
    for(size_t i = 0; i < key_len; i++) {
      if(entry[i] != key[i]) {
        match = false;
        break;
      }
    }
    if(match) {
      return entry + key_len + 1;
    }
  }

  return NULL;
}

#ifdef MOSH_TEST
void mosh_test_set_env(char** envp) {
  process_envp = envp;
}
#endif

static size_t str_len(const char* s) {
  size_t len = 0;
  while(s[len] != '\0') {
    len++;
  }
  return len;
}

static bool str_eq(const char* a, const char* b) {
  while(*a && *b) {
    if(*a++ != *b++) {
      return false;
    }
  }
  return *a == '\0' && *b == '\0';
}

static void str_copy(char* dest, size_t capacity, const char* src) {
  if(dest == NULL || capacity == 0) {
    return;
  }
  size_t idx = 0;
  if(src != NULL) {
    while(idx + 1 < capacity && src[idx] != '\0') {
      dest[idx] = src[idx];
      idx++;
    }
  }
  dest[idx] = '\0';
}

#ifdef MOSH_TEST
void mosh_test_write_bytes(int fd, const char* data, size_t length);

static void write_bytes(int fd, const char* data, size_t length) {
  if(data == NULL || length == 0) {
    return;
  }
  mosh_test_write_bytes(fd, data, length);
}
#else
static void write_bytes(int fd, const char* data, size_t length) {
  if(data == NULL || length == 0) {
    return;
  }
  syscall3(SYS_WRITE, fd, (long)data, (long)length);
}
#endif

static void write_char_stdout(char ch) {
  char tmp[1] = { ch };
  write_bytes(STDOUT_FILENO, tmp, 1);
}

static void write_str(int fd, const char* text) {
  if(text == NULL) {
    return;
  }
  size_t len = str_len(text);
  write_bytes(fd, text, len);
  if(fd == STDOUT_FILENO) {
    write_bytes(STDERR_FILENO, text, len);
  }
}

static void beep(void) {
  write_char_stdout('\a');
}

static size_t utoa(size_t value, char* out, size_t capacity) {
  if(capacity == 0) {
    return 0;
  }
  size_t len = 0;
  if(value == 0) {
    out[len++] = '0';
  } else {
    while(value > 0 && len < capacity) {
      out[len++] = (char)('0' + (value % 10));
      value /= 10;
    }
    for(size_t i = 0; i < len / 2; i++) {
      char tmp = out[i];
      out[i] = out[len - 1 - i];
      out[len - 1 - i] = tmp;
    }
  }
  return len;
}

static size_t itoa(int value, char* out, size_t capacity) {
  if(capacity == 0) {
    return 0;
  }
  size_t index = 0;
  unsigned int magnitude;
  if(value < 0) {
    if(capacity == 1) {
      return 0;
    }
    out[index++] = '-';
    magnitude = (unsigned int)(-value);
  } else {
    magnitude = (unsigned int)value;
  }

  size_t written = utoa((size_t)magnitude, &out[index], capacity - index);
  if(written == 0 && magnitude == 0) {
    if(index < capacity) {
      out[index++] = '0';
      written = 1;
    }
  }
  return index + written;
}

static void move_cursor_left(size_t count) {
  if(count == 0) {
    return;
  }
  char seq[16];
  size_t len = 0;
  seq[len++] = '\x1b';
  seq[len++] = '[';
  len += utoa(count, &seq[len], sizeof(seq) - len - 1);
  seq[len++] = 'D';
  write_bytes(STDOUT_FILENO, seq, len);
}

static void move_cursor_right(size_t count) {
  if(count == 0) {
    return;
  }
  char seq[16];
  size_t len = 0;
  seq[len++] = '\x1b';
  seq[len++] = '[';
  len += utoa(count, &seq[len], sizeof(seq) - len - 1);
  seq[len++] = 'C';
  write_bytes(STDOUT_FILENO, seq, len);
}

static void write_spaces(size_t count) {
  static const char spaces[] = "                                ";
  while(count > 0) {
    size_t chunk = count;
    if(chunk > sizeof(spaces) - 1) {
      chunk = sizeof(spaces) - 1;
    }
    write_bytes(STDOUT_FILENO, spaces, chunk);
    count -= chunk;
  }
}

typedef struct line_state_t {
  char*       buffer;
  size_t      total_capacity;
  size_t      content_capacity;
  size_t      length;
  size_t      cursor;
  size_t      rendered_length;
  const char* prompt;
  size_t      prompt_len;
  bool        needs_carriage_return;
  bool        caret_visible;
} line_state_t;

static void line_hide_caret(line_state_t* state) {
  if(!state->caret_visible) {
    return;
  }
  char fill = (state->cursor < state->length) ? state->buffer[state->cursor] : ' ';
  write_char_stdout(fill);
  move_cursor_left(1);
  state->caret_visible = false;
}

static void line_render_caret(line_state_t* state) {
  line_hide_caret(state);
  write_char_stdout('_');
  move_cursor_left(1);
  state->caret_visible = true;
}

static void line_state_init(line_state_t* state, const char* prompt, char* buffer, size_t capacity) {
  state->buffer = buffer;
  state->total_capacity = capacity;
  state->content_capacity = (capacity > 0) ? (capacity - 1) : 0;
  state->length = 0;
  state->cursor = 0;
  state->rendered_length = 0;
  state->prompt = prompt;
  state->prompt_len = (prompt != NULL) ? str_len(prompt) : 0;
  state->needs_carriage_return = true;
  state->caret_visible = false;
  if(state->buffer != NULL && state->total_capacity > 0) {
    state->buffer[0] = '\0';
  }
  if(state->prompt != NULL && state->prompt_len > 0) {
    write_bytes(STDOUT_FILENO, state->prompt, state->prompt_len);
  }
  line_render_caret(state);
}

static void line_redraw(line_state_t* state) {
  line_hide_caret(state);
  if(state->needs_carriage_return) {
    write_char_stdout('\r');
  } else {
    state->needs_carriage_return = true;
  }

  if(state->prompt != NULL && state->prompt_len > 0) {
    write_bytes(STDOUT_FILENO, state->prompt, state->prompt_len);
  }

  if(state->length > 0) {
    write_bytes(STDOUT_FILENO, state->buffer, state->length);
  }

  if(state->rendered_length > state->length) {
    size_t diff = state->rendered_length - state->length;
    write_spaces(diff);
    move_cursor_left(diff);
  }

  state->rendered_length = state->length;

  size_t tail = state->length - state->cursor;
  if(tail > 0) {
    move_cursor_left(tail);
  }
  line_render_caret(state);
}

static void line_insert_char(line_state_t* state, char ch) {
  if(state->length >= state->content_capacity) {
    beep();
    return;
  }

  size_t insert_at = state->cursor;
  bool appending = (insert_at == state->length);
  for(size_t idx = state->length + 1; idx > insert_at; idx--) {
    state->buffer[idx] = state->buffer[idx - 1];
  }
  state->buffer[insert_at] = ch;
  state->length++;
  state->cursor++;
  state->buffer[state->length] = '\0';

  if(appending) {
    write_char_stdout(ch);
    state->rendered_length = state->length;
    state->needs_carriage_return = true;
    line_render_caret(state);
  } else {
    line_redraw(state);
  }
}

static void line_backspace(line_state_t* state) {
  if(state->cursor == 0) {
    beep();
    return;
  }

  size_t remove_at = state->cursor - 1;
  for(size_t idx = remove_at; idx < state->length; idx++) {
    state->buffer[idx] = state->buffer[idx + 1];
  }
  state->cursor--;
  state->length--;
  state->buffer[state->length] = '\0';
  line_redraw(state);
}

static void line_delete_at_cursor(line_state_t* state) {
  if(state->cursor >= state->length) {
    beep();
    return;
  }
  for(size_t idx = state->cursor; idx < state->length; idx++) {
    state->buffer[idx] = state->buffer[idx + 1];
  }
  state->length--;
  state->buffer[state->length] = '\0';
  line_redraw(state);
}

static void line_cursor_left(line_state_t* state) {
  if(state->cursor == 0) {
    beep();
    return;
  }
  state->cursor--;
  line_redraw(state);
}

static void line_cursor_right(line_state_t* state) {
  if(state->cursor >= state->length) {
    beep();
    return;
  }
  state->cursor++;
  line_redraw(state);
}

static void line_cursor_home(line_state_t* state) {
  if(state->cursor == 0) {
    return;
  }
  state->cursor = 0;
  line_redraw(state);
}

static void line_cursor_end(line_state_t* state) {
  if(state->cursor == state->length) {
    return;
  }
  state->cursor = state->length;
  line_redraw(state);
}

static int read_stdin_char(void) {
  char ch = 0;
  long rc = syscall3(SYS_READ, STDIN_FILENO, (long)&ch, 1);
  if(rc <= 0) {
    return -1;
  }
  return (unsigned char)ch;
}

static char history_entries[MOSH_HISTORY_LIMIT][MOSH_MAX_LINE_LENGTH];
static size_t history_length = 0;
static size_t history_next = 0;

static void history_reset(void) {
  for(size_t i = 0; i < MOSH_HISTORY_LIMIT; i++) {
    history_entries[i][0] = '\0';
  }
  history_length = 0;
  history_next = 0;
}

static size_t history_physical_index(size_t logical_index) {
  if(history_length == 0) {
    return 0;
  }
  size_t base = (history_next + MOSH_HISTORY_LIMIT - history_length) % MOSH_HISTORY_LIMIT;
  return (base + logical_index) % MOSH_HISTORY_LIMIT;
}

static const char* history_get(size_t logical_index) {
  if(logical_index >= history_length) {
    return "";
  }
  size_t idx = history_physical_index(logical_index);
  return history_entries[idx];
}

static void history_add(const char* line) {
  if(line == NULL || line[0] == '\0') {
    return;
  }

  if(history_length > 0) {
    size_t last = history_physical_index(history_length - 1);
    if(str_eq(history_entries[last], line)) {
      return;
    }
  }

  str_copy(history_entries[history_next], MOSH_MAX_LINE_LENGTH, line);
  history_next = (history_next + 1) % MOSH_HISTORY_LIMIT;
  if(history_length < MOSH_HISTORY_LIMIT) {
    history_length++;
  }
}

enum escape_state {
  ESCAPE_NONE,
  ESCAPE_STARTED,
  ESCAPE_BRACKET,
};

static int parse_escape_number(const char* params, size_t length) {
  if(length == 0) {
    return -1;
  }
  int value = 0;
  for(size_t i = 0; i < length; i++) {
    char ch = params[i];
    if(ch < '0' || ch > '9') {
      return -1;
    }
    value = value * 10 + (ch - '0');
  }
  return value;
}

static void history_load_into(line_state_t* state, size_t logical_index) {
  const char* entry = history_get(logical_index);
  str_copy(state->buffer, state->total_capacity, entry);
  state->length = str_len(state->buffer);
  if(state->length > state->content_capacity) {
    state->length = state->content_capacity;
    state->buffer[state->length] = '\0';
  }
  state->cursor = state->length;
  line_redraw(state);
}

static size_t read_line(const char* prompt, char* buffer, size_t capacity) {
  if(buffer == NULL || capacity == 0) {
    return 0;
  }

  if(capacity > MOSH_MAX_LINE_LENGTH) {
    capacity = MOSH_MAX_LINE_LENGTH;
  }

  line_state_t state;
  line_state_init(&state, prompt, buffer, capacity);

  char scratch[MOSH_MAX_LINE_LENGTH];
  scratch[0] = '\0';
  bool scratch_active = false;

  size_t history_cursor = history_length;

  enum escape_state esc_state = ESCAPE_NONE;
  char esc_params[8];
  size_t esc_param_len = 0;
  bool swallow_lf = false;

  while(true) {
    int input = read_stdin_char();
    if(input < 0) {
      continue;
    }
    char ch = (char)input;

    if(swallow_lf) {
      swallow_lf = false;
      if(ch == '\n') {
        continue;
      }
    }

    switch(esc_state) {
      case ESCAPE_NONE:
        if(ch == '\n' || ch == '\r') {
          state.buffer[state.length] = '\0';
          line_hide_caret(&state);
          write_char_stdout('\n');
          if(ch == '\r') {
            swallow_lf = true;
          }
          return state.length;
        }
        if(ch == '\b' || ch == 0x7f) {
          line_backspace(&state);
          break;
        }
        if(ch == '\x1b') {
          esc_state = ESCAPE_STARTED;
          esc_param_len = 0;
          break;
        }
        if(ch == '\t') {
          line_insert_char(&state, ch);
          break;
        }
        if(ch >= 0x20 && ch < 0x7f) {
          line_insert_char(&state, ch);
          break;
        }
        if(ch == 0x04) {
          if(state.length == 0) {
            line_hide_caret(&state);
            return 0;
          }
          beep();
          break;
        }
        break;

      case ESCAPE_STARTED:
        if(ch == '[') {
          esc_state = ESCAPE_BRACKET;
          esc_param_len = 0;
        } else {
          esc_state = ESCAPE_NONE;
        }
        break;

      case ESCAPE_BRACKET:
        if((ch >= '0' && ch <= '9') || ch == ';') {
          if(esc_param_len + 1 < sizeof(esc_params)) {
            esc_params[esc_param_len++] = ch;
          }
          break;
        }
        esc_state = ESCAPE_NONE;
        switch(ch) {
          case 'A':
            if(history_length == 0 || history_cursor == 0) {
              beep();
              break;
            }
            if(history_cursor == history_length && !scratch_active) {
              str_copy(scratch, sizeof(scratch), state.buffer);
              scratch_active = true;
            }
            history_cursor--;
            history_load_into(&state, history_cursor);
            break;
          case 'B':
            if(history_cursor == history_length) {
              beep();
              break;
            }
            history_cursor++;
            if(history_cursor == history_length) {
              if(scratch_active) {
                str_copy(state.buffer, state.total_capacity, scratch);
                state.length = str_len(state.buffer);
                if(state.length > state.content_capacity) {
                  state.length = state.content_capacity;
                  state.buffer[state.length] = '\0';
                }
                state.cursor = state.length;
                line_redraw(&state);
              } else {
                state.length = 0;
                state.cursor = 0;
                state.buffer[0] = '\0';
                line_redraw(&state);
              }
              scratch_active = false;
            } else {
              history_load_into(&state, history_cursor);
            }
            break;
          case 'C':
            line_cursor_right(&state);
            break;
          case 'D':
            line_cursor_left(&state);
            break;
          case 'H':
            line_cursor_home(&state);
            break;
          case 'F':
            line_cursor_end(&state);
            break;
          case '~': {
            int code = parse_escape_number(esc_params, esc_param_len);
            if(code == 3) {
              line_delete_at_cursor(&state);
            } else if(code == 1 || code == 7) {
              line_cursor_home(&state);
            } else if(code == 4 || code == 8) {
              line_cursor_end(&state);
            }
            break;
          }
          default:
            break;
        }
        break;
    }
  }
}

static bool handle_builtin(const char* line) {
  if(str_eq(line, "")) {
    return true;
  }

  if(str_eq(line, "help")) {
    write_str(STDOUT_FILENO,
              "Built-ins:\n"
              "  help  - show this message\n"
              "  exit  - leave mosh\n");
    return true;
  }

  if(str_eq(line, "exit")) {
    write_str(STDOUT_FILENO, "bye\n");
    syscall1(SYS_EXIT, 0);
    return true; // Not reached
  }

  return false;
}

static bool contains_slash(const char* text) {
  for(size_t i = 0; text[i] != '\0'; i++) {
    if(text[i] == '/') {
      return true;
    }
  }
  return false;
}

static void exec_command(char** argv, size_t argc) {
  if(argv == NULL || argc == 0 || argv[0] == NULL) {
    syscall1(SYS_EXIT, 0);
    return;
  }

  char** envp = (process_envp != NULL) ? process_envp : fallback_envp;
  const char* command = argv[0];

  if(contains_slash(command)) {
    long rc = syscall3(SYS_EXECVE, (long)command, (long)argv, (long)envp);
    if(rc < 0) {
      write_str(STDERR_FILENO, "mosh: exec failed\n");
      syscall1(SYS_EXIT, 126);
    }
    return;
  }

  const char* path_value = env_get("PATH");
  if(path_value == NULL || path_value[0] == '\0') {
    path_value = DEFAULT_PATH;
  }

  size_t command_len = str_len(command);
  static char candidate[256];
  const char* segment = path_value;

  while(true) {
    size_t segment_len = 0;
    while(segment[segment_len] != '\0' && segment[segment_len] != ':') {
      segment_len++;
    }
    bool at_end = (segment[segment_len] == '\0');

    if(segment_len == 0) {
      long rc = syscall3(SYS_EXECVE, (long)command, (long)argv, (long)envp);
      if(rc >= 0) {
        return;
      }
    } else {
      bool append_slash = (segment[segment_len - 1] != '/');
      size_t required = segment_len + (append_slash ? 1 : 0) + command_len + 1;
      if(required <= sizeof(candidate)) {
        size_t pos = 0;
        for(size_t i = 0; i < segment_len; i++) {
          candidate[pos++] = segment[i];
        }
        if(append_slash) {
          candidate[pos++] = '/';
        }
        for(size_t i = 0; i < command_len; i++) {
          candidate[pos++] = command[i];
        }
        candidate[pos] = '\0';
        long rc = syscall3(SYS_EXECVE, (long)candidate, (long)argv, (long)envp);
        if(rc >= 0) {
          return;
        }
      }
    }

    if(at_end) {
      break;
    }
    segment += segment_len + 1;
  }

  write_str(STDERR_FILENO, "mosh: command not found\n");
  syscall1(SYS_EXIT, 127);
}

static void launch_command(char* line) {
  char* argv[16];
  size_t argc = 0;

  char* cursor = line;
  while(*cursor != '\0' && argc < (sizeof(argv) / sizeof(argv[0])) - 1) {
    while(*cursor == ' ' || *cursor == '\t') {
      cursor++;
    }
    if(*cursor == '\0') {
      break;
    }
    argv[argc++] = cursor;
    while(*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
      cursor++;
    }
    if(*cursor == '\0') {
      break;
    }
    *cursor++ = '\0';
  }
  argv[argc] = NULL;

  if(argc == 0) {
    return;
  }

  long pid = syscall0(SYS_FORK);
  if(pid < 0) {
    write_str(STDERR_FILENO, "mosh: fork failed\n");
    return;
  }

  if(pid == 0) {
    exec_command(argv, argc);
    syscall1(SYS_EXIT, 127);
  }

  int status = 0;
  long waited = syscall3(SYS_WAITPID, pid, (long)&status, 0);
  if(waited < 0) {
    write_str(STDERR_FILENO, "mosh: waitpid failed\n");
    return;
  }

  if(waited != pid) {
    write_str(STDERR_FILENO, "mosh: waitpid returned unexpected pid\n");
    return;
  }

  if(status != 0) {
    static const char prefix[] = "mosh: process exited with status ";
    char buffer[32];
    size_t len = itoa(status, buffer, sizeof(buffer));
    write_bytes(STDERR_FILENO, prefix, sizeof(prefix) - 1);
    if(len > 0 && len <= sizeof(buffer)) {
      write_bytes(STDERR_FILENO, buffer, len);
    }
    write_bytes(STDERR_FILENO, "\n", 1);
  }
}

static void shell_loop(void) {
  static char line_buffer[MOSH_MAX_LINE_LENGTH];

  write_str(STDOUT_FILENO, "This is mosh, the meniOS shell\nType 'help' for instructions.\n\n");

  while(true) {
    size_t length = read_line(MOSH_PROMPT, line_buffer, sizeof(line_buffer));

    if(line_buffer[0] == '\0') {
      continue;
    }

    char original_line[MOSH_MAX_LINE_LENGTH];
    str_copy(original_line, sizeof(original_line), line_buffer);
    history_add(original_line);

    if(handle_builtin(line_buffer)) {
      continue;
    }

    launch_command(line_buffer);
  }
}

#ifndef MOSH_TEST
void _start(uint64_t argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  process_envp = envp;
  history_reset();
  shell_loop();
  syscall1(SYS_EXIT, 0);
}
#endif
