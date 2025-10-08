#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <menios/syscall.h>
#ifndef MOSH_TEST
#include <menios/syscall_user.h>
#endif

#define WNOHANG 1

#define MOSH_MAX_LINE_LENGTH 256
#define MOSH_MAX_PATH        256
#define MOSH_HISTORY_LIMIT   16
#define MOSH_PROMPT_DEFAULT  "mosh:/>"
#define MOSH_PROMPT          MOSH_PROMPT_DEFAULT
#define MOSH_MAX_SEGMENTS    8
#define MOSH_MAX_ARGS        16
#define MOSH_MAX_TOKENS      128
#define MOSH_MAX_SEQUENCES   8

static char** process_envp = NULL;
static char fallback_path[] = "PATH=/bin";
static char fallback_home[] = "HOME=/";
static char fallback_pwd[]  = "PWD=/";
static char*  fallback_envp[] = { fallback_path, fallback_home, fallback_pwd, NULL };
static const char DEFAULT_PATH[] = "/bin";
static char     current_directory[MOSH_MAX_PATH];
static char     pending_input[128];
static size_t   pending_length = 0;
static size_t   pending_offset = 0;

static size_t str_len(const char* s);
static bool   str_eq(const char* a, const char* b);
static void   str_copy(char* dest, size_t capacity, const char* src);

typedef struct {
  char*  argv[MOSH_MAX_ARGS];
  size_t argc;
  char*  redirect_in;
  char*  redirect_out;
} command_segment_t;

static void write_bytes(int fd, const char* data, size_t length);
static void write_char_stdout(char ch);
static void write_str(int fd, const char* text);
static void exec_command(char** argv, size_t argc);
static long  mosh_fork(void);
static long  mosh_execve(const char* path, char* const argv[], char* const envp[]);
static const char* shell_prompt(void);

static bool parse_command_segments(char* buffer, command_segment_t* segments, size_t* segment_count);
static int  execute_pipeline(command_segment_t* segments, size_t segment_count);
static int  wait_for_children(command_segment_t* segments, size_t segment_count, long* pids, int* statuses);
static int  launch_pipeline(char* line);
static int  launch_command(char* line);
static size_t split_sequence(char* line, char* parts[], size_t max_parts);
static char* str_find_substring(char* haystack, const char* needle);

#ifdef MOSH_TEST
long mosh_test_syscall0(long number);
long mosh_test_syscall1(long number, long arg1);
long mosh_test_syscall2(long number, long arg1, long arg2);
long mosh_test_syscall3(long number, long arg1, long arg2, long arg3);

static inline long syscall0(long number) {
  return mosh_test_syscall0(number);
}

static inline long syscall1(long number, long arg1) {
  return mosh_test_syscall1(number, arg1);
}

static inline long syscall2(long number, long arg1, long arg2) {
  return mosh_test_syscall2(number, arg1, arg2);
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
  return mosh_test_syscall3(number, arg1, arg2, arg3);
}
#else
static inline long syscall0(long number) {
  return __menios_syscall0(number);
}

static inline long syscall1(long number, long arg1) {
  return __menios_syscall1(number, arg1);
}

static inline long syscall2(long number, long arg1, long arg2) {
  return __menios_syscall2(number, arg1, arg2);
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
  return __menios_syscall3(number, arg1, arg2, arg3);
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
  process_envp = (envp != NULL) ? envp : fallback_envp;
}
#endif

static char* env_find_entry(const char* key) {
  if(process_envp == NULL || key == NULL) {
    return NULL;
  }
  size_t key_len = str_len(key);
  for(size_t idx = 0; process_envp[idx] != NULL; idx++) {
    char* entry = process_envp[idx];
    size_t pos = 0;
    while(entry[pos] != '\0' && entry[pos] != '=') {
      pos++;
    }
    if(entry[pos] == '=' && pos == key_len) {
      bool match = true;
      for(size_t i = 0; i < key_len; i++) {
        if(entry[i] != key[i]) {
          match = false;
          break;
        }
      }
      if(match) {
        return entry;
      }
    }
  }
  return NULL;
}

static void env_set(const char* key, const char* value) {
  char* entry = env_find_entry(key);
  if(entry == NULL || value == NULL) {
    return;
  }
  size_t key_len = str_len(key);
  if(entry[key_len] != '=') {
    return;
  }
  size_t value_len = str_len(value);
  size_t idx = key_len + 1;
  for(size_t i = 0; i < value_len; i++) {
    entry[idx + i] = value[i];
  }
  entry[idx + value_len] = '\0';
}

static size_t str_len(const char* s) {
  return s ? strlen(s) : 0;
}

static bool str_eq(const char* a, const char* b) {
  if(a == NULL || b == NULL) {
    return a == b;
  }
  return strcmp(a, b) == 0;
}

static int str_ncmp(const char* a, const char* b, size_t length) {
  if(length == 0) {
    return 0;
  }
  if(a == NULL || b == NULL) {
    return (a == b) ? 0 : (a == NULL ? -1 : 1);
  }
  return strncmp(a, b, length);
}

static void str_copy(char* dest, size_t capacity, const char* src) {
  if(dest == NULL || capacity == 0) {
    return;
  }
  if(src == NULL) {
    dest[0] = '\0';
    return;
  }
  size_t idx = 0;
  while(idx + 1 < capacity && src[idx] != '\0') {
    dest[idx] = src[idx];
    idx++;
  }
  dest[idx] = '\0';
}

static bool build_combined_path(const char* base, const char* path, char* out, size_t capacity) {
  if(path != NULL && path[0] == '/') {
    str_copy(out, capacity, path);
    return out[0] != '\0';
  }

  size_t len = 0;
  if(base == NULL || base[0] == '\0') {
    out[len++] = '/';
  } else {
    while(base[len] != '\0' && len + 1 < capacity) {
      out[len] = base[len];
      len++;
    }
    if(len == 0) {
      out[len++] = '/';
    }
  }
  if(len >= capacity) {
    out[capacity - 1] = '\0';
    return false;
  }
  out[len] = '\0';

  if(path == NULL || path[0] == '\0') {
    return true;
  }

  if(len > 1 && out[len - 1] != '/') {
    if(len + 1 >= capacity) {
      return false;
    }
    out[len++] = '/';
    out[len] = '\0';
  }

  size_t idx = 0;
  while(path[idx] != '\0' && len + 1 < capacity) {
    out[len++] = path[idx++];
  }
  out[len] = '\0';
  if(path[idx] != '\0') {
    return false;
  }
  return true;
}

static bool canonicalize_path(const char* input, char* output, size_t capacity) {
  if(output == NULL || capacity == 0) {
    return false;
  }
  size_t out_len = 0;
  size_t depth = 0;
  size_t depth_pos[64];

  output[out_len++] = '/';
  output[out_len] = '\0';

  size_t i = 0;
  if(input[0] == '/') {
    i++;
  }

  while(true) {
    while(input[i] == '/') {
      i++;
    }
    if(input[i] == '\0') {
      break;
    }
    size_t start = i;
    while(input[i] != '\0' && input[i] != '/') {
      i++;
    }
    size_t seg_len = i - start;
    if(seg_len == 0) {
      break;
    }
    if(seg_len == 1 && input[start] == '.') {
      continue;
    }
    if(seg_len == 2 && input[start] == '.' && input[start + 1] == '.') {
      if(depth > 0) {
        out_len = depth_pos[depth - 1];
        output[out_len] = '\0';
        depth--;
      }
      continue;
    }
    if(out_len > 1) {
      if(out_len + 1 >= capacity) {
        return false;
      }
      output[out_len++] = '/';
    }
    if(depth >= sizeof(depth_pos) / sizeof(depth_pos[0])) {
      return false;
    }
    depth_pos[depth++] = out_len;
    for(size_t j = 0; j < seg_len; j++) {
      if(out_len + 1 >= capacity) {
        return false;
      }
      output[out_len++] = input[start + j];
    }
    output[out_len] = '\0';
  }

  if(out_len == 1) {
    output[0] = '/';
    output[1] = '\0';
  }

  return true;
}

static bool normalize_path(const char* base, const char* path, char* out, size_t capacity) {
  char combined[MOSH_MAX_PATH];
  if(!build_combined_path(base, path, combined, sizeof(combined))) {
    return false;
  }
  return canonicalize_path(combined, out, capacity);
}

static void init_segment(command_segment_t* segment) {
  segment->argc = 0;
  segment->redirect_in = NULL;
  segment->redirect_out = NULL;
  for(size_t i = 0; i < MOSH_MAX_ARGS; i++) {
    segment->argv[i] = NULL;
  }
}

static bool parse_command_segments(char* buffer, command_segment_t* segments, size_t* segment_count) {
  if(buffer == NULL || segments == NULL || segment_count == NULL) {
    return false;
  }

  char* tokens[MOSH_MAX_TOKENS];
  size_t token_count = 0;
  char* cursor = buffer;
  while(*cursor != '\0') {
    while(*cursor == ' ' || *cursor == '\t') {
      cursor++;
    }
    if(*cursor == '\0') {
      break;
    }
    if(token_count >= MOSH_MAX_TOKENS) {
      return false;
    }
    tokens[token_count++] = cursor;
    while(*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
      cursor++;
    }
    if(*cursor != '\0') {
      *cursor++ = '\0';
    }
  }

  if(token_count == 0) {
    return false;
  }

  size_t seg_idx = 0;
  init_segment(&segments[0]);
  command_segment_t* current = &segments[0];

  for(size_t i = 0; i < token_count; i++) {
    char* tok = tokens[i];
    if(tok[0] == '\0') {
      continue;
    }

    if(tok[0] == '|' && tok[1] == '\0') {
      if(current->argc == 0) {
        return false;
      }
      seg_idx++;
      if(seg_idx >= MOSH_MAX_SEGMENTS) {
        return false;
      }
      init_segment(&segments[seg_idx]);
      current = &segments[seg_idx];
      continue;
    }

    if(tok[0] == '<' && tok[1] == '\0') {
      if(i + 1 >= token_count) {
        return false;
      }
      if(current->redirect_in != NULL) {
        return false;
      }
      current->redirect_in = tokens[++i];
      continue;
    }

    if(tok[0] == '>' && tok[1] == '\0') {
      if(i + 1 >= token_count) {
        return false;
      }
      if(current->redirect_out != NULL) {
        return false;
      }
      current->redirect_out = tokens[++i];
      continue;
    }

    if(current->argc >= MOSH_MAX_ARGS - 1) {
      return false;
    }
    current->argv[current->argc++] = tok;
    current->argv[current->argc] = NULL;
  }

  if(segments[seg_idx].argc == 0) {
    return false;
  }

  *segment_count = seg_idx + 1;
  return true;
}

static inline void close_fd_if_needed(int fd) {
  if(fd >= 0) {
    close(fd);
  }
}

static int wait_for_children(command_segment_t* segments,
                             size_t segment_count,
                             long* pids,
                             int* statuses) {
  size_t remaining = segment_count;
  for(size_t i = 0; i < segment_count; i++) {
    statuses[i] = 0;
  }

  while(remaining > 0) {
    bool progress = false;

    for(size_t i = 0; i < segment_count; i++) {
      long pid = pids[i];
      if(pid <= 0) {
        continue;
      }

      int status = 0;
      long waited = syscall3(SYS_WAITPID, pid, (long)&status, WNOHANG);
      if(waited == pid) {
        pids[i] = -pid;
        statuses[i] = status;
        remaining--;
        progress = true;
        if(status == 127 && segments[i].argc > 0) {
          const char prefix[] = "mosh: command not found: ";
          write_bytes(STDOUT_FILENO, prefix, sizeof(prefix) - 1);
          write_bytes(STDOUT_FILENO, segments[i].argv[0], str_len(segments[i].argv[0]));
          write_bytes(STDOUT_FILENO, "\n", 1);
        }
        continue;
      }

      if(waited < 0 && waited != -ECHILD) {
        write_str(STDOUT_FILENO, "mosh: waitpid failed\n");
        pids[i] = -pid;
        statuses[i] = (int)waited;
        remaining--;
        progress = true;
      }
    }

    if(remaining == 0) {
      break;
    }

    long polled = syscall0(SYS_STDIN_POLL);
    if(polled >= 0) {
      char ch = (char)polled;
      if(ch == 3) {
        pending_length = 0;
        pending_offset = 0;
        write_str(STDOUT_FILENO, "^C\n");
        for(size_t i = 0; i < segment_count; i++) {
          if(pids[i] > 0) {
            syscall2(SYS_PROC_KILL, pids[i], 130);
          }
        }
      } else {
        if(pending_length + 1 < sizeof(pending_input)) {
          pending_input[pending_length++] = ch;
          pending_input[pending_length] = '\0';
          write_char_stdout(ch);
        }
      }
    }

    if(!progress) {
      syscall2(SYS_SLEEP, 1000, 0);
    }
  }

  return statuses[segment_count - 1];
}

static int execute_pipeline(command_segment_t* segments, size_t segment_count) {
  long pids[MOSH_MAX_SEGMENTS] = {0};
  int statuses[MOSH_MAX_SEGMENTS] = {0};
  int prev_read = -1;
  size_t started = 0;

  for(size_t i = 0; i < segment_count; i++) {
    int pipe_fds[2] = { -1, -1 };
    if(i + 1 < segment_count) {
      int tmp[2];
      int rc = pipe(tmp);
      if(rc < 0) {
        write_str(STDOUT_FILENO, "mosh: failed to create pipe\n");
        close_fd_if_needed(prev_read);
        if(started > 0) {
          wait_for_children(segments, started, pids, statuses);
        }
        return (int)rc;
      }
      pipe_fds[0] = tmp[0];
      pipe_fds[1] = tmp[1];
    }

    long fork_rc = mosh_fork();
    if(fork_rc < 0) {
      write_str(STDOUT_FILENO, "mosh: fork failed\n");
      close_fd_if_needed(pipe_fds[0]);
      close_fd_if_needed(pipe_fds[1]);
      close_fd_if_needed(prev_read);
      if(started > 0) {
        wait_for_children(segments, started, pids, statuses);
      }
      return (int)fork_rc;
    }

    pid_t pid = (pid_t)fork_rc;

    if(pid == 0) {
      if(segments[i].redirect_in != NULL) {
        char absolute[MOSH_MAX_PATH];
        if(!normalize_path(current_directory, segments[i].redirect_in, absolute, sizeof(absolute))) {
          write_str(STDOUT_FILENO, "mosh: invalid input path\n");
          _exit(1);
        }
        int fd = open(absolute, O_RDONLY);
        if(fd < 0) {
          write_str(STDOUT_FILENO, "mosh: failed to open input file\n");
          _exit(1);
        }
        dup2(fd, STDIN_FILENO);
        if(fd != STDIN_FILENO) {
          close(fd);
        }
      } else if(prev_read >= 0) {
        dup2(prev_read, STDIN_FILENO);
      }

      if(segments[i].redirect_out != NULL) {
        char absolute[MOSH_MAX_PATH];
        if(!normalize_path(current_directory, segments[i].redirect_out, absolute, sizeof(absolute))) {
          write_str(STDOUT_FILENO, "mosh: invalid redirection path\n");
          _exit(1);
        }
        int fd = open(absolute, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd < 0) {
          write_str(STDOUT_FILENO, "mosh: failed to open redirection target\n");
          _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        if(fd != STDOUT_FILENO) {
          close(fd);
        }
      } else if(pipe_fds[1] >= 0) {
        dup2(pipe_fds[1], STDOUT_FILENO);
      }

      close_fd_if_needed(prev_read);
      close_fd_if_needed(pipe_fds[0]);
      close_fd_if_needed(pipe_fds[1]);

      exec_command(segments[i].argv, segments[i].argc);
      _exit(127);
    }

    pids[started++] = (long)pid;
    if(prev_read >= 0) {
      close_fd_if_needed(prev_read);
    }
    if(pipe_fds[1] >= 0) {
      close_fd_if_needed(pipe_fds[1]);
    }
    prev_read = pipe_fds[0];
  }

  close_fd_if_needed(prev_read);

  int last_status = wait_for_children(segments, segment_count, pids, statuses);
  return last_status;
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
  (void)write(fd, data, length);
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

#ifdef MOSH_TEST
static long mosh_fork(void) {
  return syscall0(SYS_FORK);
}

static long mosh_execve(const char* path, char* const argv[], char* const envp[]) {
  return syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
}
#else
static long mosh_fork(void) {
  pid_t pid = fork();
  return (pid < 0) ? -1 : (long)pid;
}

static long mosh_execve(const char* path, char* const argv[], char* const envp[]) {
  return (long)execve(path, argv, envp);
}
#endif

static size_t format_unsigned_value(size_t value, char* out, size_t capacity) {
  if(out == NULL || capacity == 0) {
    return 0;
  }

  char tmp[32];
  size_t pos = 0;
  if(value == 0) {
    tmp[pos++] = '0';
  } else {
    while(value > 0 && pos < sizeof(tmp)) {
      tmp[pos++] = (char)('0' + (value % 10));
      value /= 10;
    }
  }

  size_t len = 0;
  while(pos > 0 && len + 1 < capacity) {
    out[len++] = tmp[--pos];
  }
  out[len] = '\0';
  return len;
}

static size_t format_signed_value(int value, char* out, size_t capacity) {
  if(out == NULL || capacity == 0) {
    return 0;
  }

  size_t index = 0;
  size_t magnitude;
  if(value < 0) {
    if(index + 1 >= capacity) {
      out[0] = '\0';
      return 0;
    }
    out[index++] = '-';
    magnitude = (size_t)(-(long)value);
  } else {
    magnitude = (size_t)value;
  }

  size_t written = format_unsigned_value(magnitude, &out[index], capacity - index);
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
  if(len + 1 >= sizeof(seq)) {
    return;
  }
  len += format_unsigned_value(count, &seq[len], sizeof(seq) - len - 1);
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
  if(len + 1 >= sizeof(seq)) {
    return;
  }
  len += format_unsigned_value(count, &seq[len], sizeof(seq) - len - 1);
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
  long rc = read(STDIN_FILENO, &ch, 1);
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
    int input = -1;
    if(pending_offset < pending_length) {
      input = (unsigned char)pending_input[pending_offset++];
      if(pending_offset >= pending_length) {
        pending_offset = 0;
        pending_length = 0;
      }
    } else {
      input = read_stdin_char();
      if(input < 0) {
        continue;
      }
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
  size_t idx = 0;
  while(line[idx] == ' ' || line[idx] == '\t') {
    idx++;
  }

  const char* command_start = line + idx;
  while(line[idx] != '\0' && line[idx] != ' ' && line[idx] != '\t') {
    idx++;
  }

  size_t command_len = (size_t)(line + idx - command_start);
  if(command_len == 0) {
    return true;
  }

  while(line[idx] == ' ' || line[idx] == '\t') {
    idx++;
  }
  const char* arguments = line + idx;

  if(command_len == 4 && str_ncmp(command_start, "help", 4) == 0) {
    write_str(STDOUT_FILENO,
              "Built-ins:\n"
              "  help  - show this message\n"
              "  exit  - leave mosh\n"
              "  pwd   - print current directory\n"
              "  echo  - print arguments\n"
              "  cd    - change directory (limited)\n");
    return true;
  }

  if(command_len == 4 && str_ncmp(command_start, "exit", 4) == 0) {
    write_str(STDOUT_FILENO, "bye\n");
    _exit(0);
    return true; // Not reached
  }

  if(command_len == 3 && str_ncmp(command_start, "pwd", 3) == 0) {
    write_str(STDOUT_FILENO, current_directory);
    write_str(STDOUT_FILENO, "\n");
    return true;
  }

  if(command_len == 4 && str_ncmp(command_start, "echo", 4) == 0) {
    bool has_redirection = false;
    for(size_t i = 0; arguments[i] != '\0'; i++) {
      char ch = arguments[i];
      if(ch == '>' || ch == '<' || ch == '|') {
        has_redirection = true;
        break;
      }
    }
    if(has_redirection) {
      return false;
    }
    write_str(STDOUT_FILENO, arguments);
    write_str(STDOUT_FILENO, "\n");
    return true;
  }

  if(command_len == 2 && str_ncmp(command_start, "cd", 2) == 0) {
    const char* target = arguments;
    while(*target == ' ' || *target == '\t') {
      target++;
    }
    char resolved[MOSH_MAX_PATH];
    if(target[0] == '\0') {
      const char* home = env_get("HOME");
      if(home == NULL || home[0] == '\0') {
        home = "/";
      }
      if(!normalize_path(current_directory, home, resolved, sizeof(resolved))) {
        write_str(STDOUT_FILENO, "mosh: cd: invalid path\n");
        return true;
      }
    } else {
      const char* extra = target;
      size_t consumed = 0;
      while(target[consumed] != '\0' && target[consumed] != ' ' && target[consumed] != '\t') {
        consumed++;
      }
      char temp[MOSH_MAX_PATH];
      if(consumed >= sizeof(temp)) {
        write_str(STDOUT_FILENO, "mosh: cd: path too long\n");
        return true;
      }
      for(size_t i = 0; i < consumed; i++) {
        temp[i] = extra[i];
      }
      temp[consumed] = '\0';
      if(!normalize_path(current_directory, temp, resolved, sizeof(resolved))) {
        write_str(STDOUT_FILENO, "mosh: cd: invalid path\n");
        return true;
      }
      while(target[consumed] == ' ' || target[consumed] == '\t') {
        consumed++;
      }
      if(target[consumed] != '\0') {
        write_str(STDOUT_FILENO, "mosh: cd: too many arguments\n");
        return true;
      }
    }

    long rc = syscall3(SYS_LISTDIR, (long)resolved, 0, 0);
    if(rc < 0) {
      write_str(STDOUT_FILENO, "mosh: cd: unable to access directory\n");
      return true;
    }

    str_copy(current_directory, sizeof(current_directory), resolved);
    env_set("PWD", current_directory);
    return true;
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
    _exit(0);
    return;
  }

  char** envp = (process_envp != NULL) ? process_envp : fallback_envp;
  const char* command = argv[0];

  if(contains_slash(command)) {
    long rc = mosh_execve(command, argv, envp);
    if(rc < 0) {
      write_str(STDERR_FILENO, "mosh: exec failed\n");
      _exit(126);
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
      long rc = mosh_execve(command, argv, envp);
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
        long rc = mosh_execve(candidate, argv, envp);
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

  _exit(127);
}

static char* ltrim(char* text) {
  while(*text == ' ' || *text == '\t') {
    text++;
  }
  return text;
}

static void rtrim(char* text) {
  size_t len = str_len(text);
  while(len > 0) {
    char ch = text[len - 1];
    if(ch != ' ' && ch != '\t') {
      break;
    }
    text[--len] = '\0';
  }
}

static size_t split_sequence(char* line, char* parts[], size_t max_parts) {
  size_t count = 0;
  char* cursor = line;

  while(true) {
    cursor = ltrim(cursor);
    if(*cursor == '\0') {
      break;
    }
    if(count >= max_parts) {
      return count;
    }

    char* segment_start = cursor;
    char* op = str_find_substring(cursor, "&&");
    if(op != NULL) {
      *op = '\0';
      op[1] = ' ';
      rtrim(segment_start);
      parts[count++] = segment_start;
      cursor = op + 2;
      continue;
    }

    rtrim(segment_start);
    parts[count++] = segment_start;
    break;
  }

  for(size_t i = 0; i < count; i++) {
    parts[i] = ltrim(parts[i]);
    rtrim(parts[i]);
  }

  return count;
}

static int launch_pipeline(char* line) {
  command_segment_t segments[MOSH_MAX_SEGMENTS];
  for(size_t i = 0; i < MOSH_MAX_SEGMENTS; i++) {
    init_segment(&segments[i]);
  }

  char expanded[MOSH_MAX_LINE_LENGTH * 3];
  size_t idx = 0;
  for(size_t i = 0; line[i] != '\0' && idx + 3 < sizeof(expanded); i++) {
    char ch = line[i];
    if(ch == '|' || ch == '<' || ch == '>') {
      expanded[idx++] = ' ';
      expanded[idx++] = ch;
      expanded[idx++] = ' ';
    } else {
      expanded[idx++] = ch;
    }
  }
  expanded[idx] = '\0';

  size_t segment_count = 0;
  if(!parse_command_segments(expanded, segments, &segment_count)) {
    write_str(STDOUT_FILENO, "mosh: syntax error\n");
    return 1;
  }

  int status = execute_pipeline(segments, segment_count);
  if(status > 0 && status != 127 && status != 130) {
    static const char prefix[] = "mosh: process exited with status ";
    char buffer[32];
    size_t len = format_signed_value(status, buffer, sizeof(buffer));
    write_bytes(STDERR_FILENO, prefix, sizeof(prefix) - 1);
    if(len > 0 && len <= sizeof(buffer)) {
      write_bytes(STDERR_FILENO, buffer, len);
    }
    write_bytes(STDERR_FILENO, "\n", 1);
  }

  return status;
}

static int launch_command(char* line) {
  char* parts[MOSH_MAX_SEQUENCES];
  size_t count = split_sequence(line, parts, MOSH_MAX_SEQUENCES);
  if(count == 0) {
    return 0;
  }

  int last_status = 0;
  for(size_t i = 0; i < count; i++) {
    char* segment = ltrim(parts[i]);
    if(segment[0] == '\0') {
      write_str(STDOUT_FILENO, "mosh: syntax error\n");
      return 1;
    }
    last_status = launch_pipeline(segment);
    if(last_status != 0) {
      break;
    }
  }

  return last_status;
}

static const char* shell_prompt(void) {
  static char prompt[MOSH_MAX_PATH + 16];
  const char prefix[] = "mosh:";
  const char suffix[] = "> ";
  const char* dir = current_directory;

  if(dir == NULL || dir[0] == '\0') {
    dir = "/";
  }

  size_t offset = 0;
  for(size_t i = 0; i < sizeof(prefix) - 1 && offset < sizeof(prompt) - 1; i++) {
    prompt[offset++] = prefix[i];
  }

  size_t dir_len = str_len(dir);
  size_t max_dir_len = 0;
  if(offset < sizeof(prompt)) {
    max_dir_len = sizeof(prompt) - offset - (sizeof(suffix) - 1) - 1;
  }
  if(dir_len > max_dir_len) {
    dir_len = max_dir_len;
  }

  for(size_t i = 0; i < dir_len && offset < sizeof(prompt) - 1; i++) {
    prompt[offset++] = dir[i];
  }

  for(size_t i = 0; i < sizeof(suffix) - 1 && offset < sizeof(prompt) - 1; i++) {
    prompt[offset++] = suffix[i];
  }

  prompt[offset] = '\0';
  return prompt;
}

static void shell_loop(void) {
  static char line_buffer[MOSH_MAX_LINE_LENGTH];

  write_str(STDOUT_FILENO, "This is mosh, the meniOS shell\nType 'help' for instructions.\n\n");

  while(true) {
    const char* prompt = shell_prompt();
    size_t length = read_line(prompt, line_buffer, sizeof(line_buffer));

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
int main(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  process_envp = envp;
  if(process_envp == NULL) {
    process_envp = fallback_envp;
  }
  str_copy(current_directory, sizeof(current_directory), "/");
  const char* initial_pwd = env_get("PWD");
  if(initial_pwd != NULL && initial_pwd[0] != '\0') {
    if(!normalize_path("/", initial_pwd, current_directory, sizeof(current_directory))) {
      str_copy(current_directory, sizeof(current_directory), "/");
    }
  }
  env_set("PWD", current_directory);
  history_reset();
  shell_loop();
  return 0;
}
#endif
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#include <stddef.h>
static char* str_find_substring(char* haystack, const char* needle) {
  if(needle[0] == '\0') {
    return haystack;
  }

  size_t i = 0;
  while(haystack[i] != '\0') {
    size_t j = 0;
    while(needle[j] != '\0' && haystack[i + j] == needle[j]) {
      j++;
    }
    if(needle[j] == '\0') {
      return &haystack[i];
    }
    i++;
  }
  return NULL;
}
