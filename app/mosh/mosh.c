#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <menios/syscall.h>
#ifndef MOSH_TEST
#include <menios/syscall_user.h>
#endif

#define MOSH_MAX_LINE_LENGTH   256
#define MOSH_MAX_PATH          256
#define MOSH_HISTORY_LIMIT     16
#define MOSH_PROMPT_DEFAULT    "mosh:/>"
#define MOSH_PROMPT            MOSH_PROMPT_DEFAULT
#define MOSH_MAX_SEGMENTS      8
#define MOSH_MAX_ARGS          16
#define MOSH_MAX_TOKENS        128
#define MOSH_MAX_SEQUENCES     8
#define MOSH_MAX_COMPLETIONS   64
#define MOSH_MAX_ENV_VARS      64
#define MOSH_MAX_VARS          64
#define MOSH_MAX_VAR_NAME      32
#define MOSH_MAX_VAR_VALUE     256
#define MOSH_MAX_FUNCTIONS     32
#define MOSH_MAX_FUNCTION_BODY (MOSH_MAX_LINE_LENGTH * 4)
#define MOSH_MAX_POSITIONAL    9
#define MOSH_POSITIONAL_STACK  8

typedef struct line_state_t line_state_t;

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
static bool     env_heap_flags[MOSH_MAX_ENV_VARS];
static char*    env_storage[MOSH_MAX_ENV_VARS + 1];
static bool     env_storage_active = false;

static size_t str_len(const char* s);
static bool   str_eq(const char* a, const char* b);
static void   str_copy(char* dest, size_t capacity, const char* src);
static bool   strip_background_marker(char* text);
static void   debug_write_ptr(int fd, const void* ptr);
static void   debug_log_expand_start(const char* input, char* output, size_t capacity);
static void   debug_log_expand_dollar(const char* cursor);
static void   debug_dump_bytes(const char* label, const char* data);
static bool   debug_ptr_readable(const void* ptr);
static void   print_exec_error(const char* command, int err, bool not_found);
static bool   compute_timespec_diff(const struct timespec* start,
                                    const struct timespec* end,
                                    long long* out_seconds,
                                    long long* out_nanoseconds);
static void   print_duration_line(const char* label,
                                  long long seconds,
                                  long long nanoseconds);

typedef struct {
  char*  argv[MOSH_MAX_ARGS];
  size_t argc;
  char*  redirect_in;
  char*  redirect_out;
  char*  redirect_err;
  bool   redirect_out_append;
  bool   redirect_err_append;
  bool   redirect_err_to_stdout;
  bool   redirect_out_to_stderr;
} command_segment_t;

typedef struct {
  bool   active;
  size_t token_start;
  size_t inserted_length;
  size_t match_count;
  size_t current_index;
  char   dir_prefix[MOSH_MAX_PATH];
  char   matches[MOSH_MAX_COMPLETIONS][MOSH_MAX_PATH];
  bool   match_is_dir[MOSH_MAX_COMPLETIONS];
} completion_context_t;

static completion_context_t completion_ctx;

#define MOSH_MAX_JOBS 16

typedef enum {
  JOB_STATE_RUNNING = 0,
  JOB_STATE_STOPPED,
  JOB_STATE_DONE
} job_state_t;

typedef struct {
  bool        in_use;
  int         id;
  job_state_t state;
  bool        background;
  bool        foreground;
  bool        notified_done;
  char        command[MOSH_MAX_LINE_LENGTH];
  size_t      segment_count;
  long        pids[MOSH_MAX_SEGMENTS];
  int         statuses[MOSH_MAX_SEGMENTS];
  char        argv0[MOSH_MAX_SEGMENTS][MOSH_MAX_PATH];
  int         last_status;
} job_t;

typedef struct {
  bool in_use;
  char name[MOSH_MAX_VAR_NAME];
  char value[MOSH_MAX_VAR_VALUE];
} shell_var_t;

typedef struct {
  bool in_use;
  char name[MOSH_MAX_VAR_NAME];
  char body[MOSH_MAX_FUNCTION_BODY];
} shell_function_t;

typedef struct {
  bool had_value;
  char name[MOSH_MAX_VAR_NAME];
  char value[MOSH_MAX_VAR_VALUE];
} shell_var_snapshot_t;

typedef struct {
  bool        had_zero;
  char        zero_value[MOSH_MAX_VAR_VALUE];
  bool        had_count;
  char        count_value[MOSH_MAX_VAR_VALUE];
  struct {
    bool had_value;
    char value[MOSH_MAX_VAR_VALUE];
  } positional[MOSH_MAX_POSITIONAL + 1];
} positional_frame_t;

static job_t jobs[MOSH_MAX_JOBS];
static int next_job_id = 1;
static job_t* current_job = NULL;
static int shell_last_status = 0;
static shell_var_t shell_vars[MOSH_MAX_VARS];
static shell_function_t shell_functions[MOSH_MAX_FUNCTIONS];
static positional_frame_t positional_stack[MOSH_POSITIONAL_STACK];
static size_t positional_depth = 0;
#ifdef MOSH_TEST
static int test_counter_limit = 0;
static int test_counter_value = 0;
#endif

static int  decode_wait_status(int status);
static int  encode_raw_status_from_code(int code);
static void shell_set_status_code(int code);
static void shell_set_status_from_raw(int status);

typedef enum {
  JOB_EVENT_NONE = 0,
  JOB_EVENT_EXITED,
  JOB_EVENT_STOPPED,
  JOB_EVENT_CONTINUED
} job_event_t;

static job_event_t job_poll_status(job_t* job, bool block);
static inline long syscall2(long number, long arg1, long arg2);
static size_t format_unsigned_value(size_t value, char* out, size_t capacity);
static size_t format_signed_value(int value, char* out, size_t capacity);
static void jobs_poll_updates(bool print_notifications);
static void jobs_print_list(void);
static int job_raw_status(const job_t* job);

typedef enum {
  SEQ_NONE = 0,
  SEQ_AND,
  SEQ_OR,
  SEQ_SEMI
} sequence_op_t;

static volatile int sigint_requested = 0;
static volatile int sigint_print_pending = 0;
static volatile int sigtstp_requested = 0;
static volatile int sigtstp_print_pending = 0;

static void write_bytes(int fd, const char* data, size_t length);
static void write_char_stdout(char ch);
static void write_str(int fd, const char* text);
static void exec_command(char** argv, size_t argc);
static long  mosh_fork(void);
static long  mosh_execve(const char* path, char* const argv[], char* const envp[]);
static const char* shell_prompt(void);

typedef struct {
  bool   active;
  bool   have_match;
  bool   saved_valid;
  char   query[MOSH_MAX_LINE_LENGTH];
  size_t query_len;
  size_t search_index;
  size_t match_index;
  char   saved_buffer[MOSH_MAX_LINE_LENGTH];
  size_t saved_length;
  size_t saved_cursor;
} reverse_search_state_t;

static reverse_search_state_t reverse_search;

static void completion_reset(void) {
  completion_ctx.active = false;
  completion_ctx.match_count = 0;
  completion_ctx.current_index = 0;
  completion_ctx.inserted_length = 0;
  completion_ctx.dir_prefix[0] = '\0';
}

static job_t* job_find_by_id(int id) {
  for(size_t i = 0; i < MOSH_MAX_JOBS; i++) {
    if(jobs[i].in_use && jobs[i].id == id) {
      return &jobs[i];
    }
  }
  return NULL;
}

static job_t* job_find_latest(bool prefer_stopped) {
  job_t* candidate = NULL;
  for(size_t i = 0; i < MOSH_MAX_JOBS; i++) {
    if(!jobs[i].in_use) {
      continue;
    }
    if(prefer_stopped && jobs[i].state == JOB_STATE_STOPPED) {
      if(candidate == NULL || jobs[i].id > candidate->id) {
        candidate = &jobs[i];
      }
      continue;
    }
    if(!prefer_stopped) {
      if(candidate == NULL || jobs[i].id > candidate->id) {
        candidate = &jobs[i];
      }
    }
  }
  return candidate;
}

static job_t* job_allocate(const char* command,
                           command_segment_t* segments,
                           size_t segment_count,
                           long* pids,
                           bool background) {
  size_t slot = MOSH_MAX_JOBS;
  for(size_t i = 0; i < MOSH_MAX_JOBS; i++) {
    if(!jobs[i].in_use) {
      slot = i;
      break;
    }
  }
  if(slot == MOSH_MAX_JOBS) {
    return NULL;
  }

  job_t* job = &jobs[slot];
  memset(job, 0, sizeof(*job));
  job->in_use = true;
  job->id = next_job_id++;
  if(next_job_id < 0) {
    next_job_id = 1;
  }
  job->state = JOB_STATE_RUNNING;
  job->background = background;
  job->foreground = !background;
  job->segment_count = segment_count;
  job->last_status = 0;
  if(command != NULL) {
    str_copy(job->command, sizeof(job->command), command);
  } else {
    job->command[0] = '\0';
  }
  for(size_t i = 0; i < segment_count && i < MOSH_MAX_SEGMENTS; i++) {
    job->pids[i] = pids ? pids[i] : 0;
    job->statuses[i] = 0;
    if(segments != NULL && segments[i].argc > 0 && segments[i].argv[0] != NULL) {
      str_copy(job->argv0[i], sizeof(job->argv0[i]), segments[i].argv[0]);
    } else {
      job->argv0[i][0] = '\0';
    }
  }
  for(size_t i = segment_count; i < MOSH_MAX_SEGMENTS; i++) {
    job->pids[i] = 0;
    job->statuses[i] = 0;
    job->argv0[i][0] = '\0';
  }
  return job;
}

static void job_release(job_t* job) {
  if(job == NULL) {
    return;
  }
  memset(job, 0, sizeof(*job));
}

static const char* job_state_label(const job_t* job) {
  switch(job->state) {
    case JOB_STATE_RUNNING:
      return job->background ? "Running" : "Running";
    case JOB_STATE_STOPPED:
      return "Stopped";
    case JOB_STATE_DONE:
      return "Done";
  }
  return "Unknown";
}

static void job_send_signal(job_t* job, int signo) {
  if(job == NULL) {
    return;
  }
  for(size_t i = 0; i < job->segment_count; i++) {
    long pid = job->pids[i];
    if(pid > 0) {
      syscall2(SYS_KILL, pid, signo);
    }
  }
}

static void job_print_notification(job_t* job, const char* status_text) {
  if(job == NULL || status_text == NULL) {
    return;
  }
  write_char_stdout('\n');
  char buffer[64];
  size_t len = 0;
  if(len + 1 < sizeof(buffer)) {
    buffer[len++] = '[';
  }
  len += format_unsigned_value((size_t)job->id, &buffer[len], sizeof(buffer) - len - 1);
  if(len + 2 < sizeof(buffer)) {
    buffer[len++] = ']';
    buffer[len++] = ' ';
  }
  const char* text = status_text;
  while(*text != '\0' && len + 1 < sizeof(buffer)) {
    buffer[len++] = *text++;
  }
  if(len + 1 < sizeof(buffer)) {
    buffer[len++] = ' ';
  }
  buffer[len] = '\0';
  write_bytes(STDOUT_FILENO, buffer, len);
  write_str(STDOUT_FILENO, job->command);
  write_char_stdout('\n');
}

static void jobs_print_list(void) {
  for(size_t i = 0; i < MOSH_MAX_JOBS; i++) {
    if(!jobs[i].in_use) {
      continue;
    }
    char header[64];
    size_t len = 0;
    if(len + 1 < sizeof(header)) {
      header[len++] = '[';
    }
    len += format_unsigned_value((size_t)jobs[i].id, &header[len], sizeof(header) - len - 1);
    if(len + 2 < sizeof(header)) {
      header[len++] = ']';
      header[len++] = ' ';
    }
    const char* state = job_state_label(&jobs[i]);
    while(state != NULL && *state != '\0' && len + 1 < sizeof(header)) {
      header[len++] = *state++;
    }
    if(len + 1 < sizeof(header)) {
      header[len++] = ' ';
    }
    header[len] = '\0';
    write_bytes(STDOUT_FILENO, header, len);
    write_str(STDOUT_FILENO, jobs[i].command);
    write_char_stdout('\n');
  }
}

static int job_wait_temporary(const char* command,
                              command_segment_t* segments,
                              size_t segment_count,
                              long* pids) {
  job_t temp;
  memset(&temp, 0, sizeof(temp));
  temp.in_use = false;
  temp.id = 0;
  temp.state = JOB_STATE_RUNNING;
  temp.foreground = true;
  temp.background = false;
  temp.segment_count = segment_count;
  if(command != NULL) {
    str_copy(temp.command, sizeof(temp.command), command);
  }
  for(size_t i = 0; i < segment_count && i < MOSH_MAX_SEGMENTS; i++) {
    temp.pids[i] = pids[i];
    temp.statuses[i] = 0;
    if(segments != NULL && segments[i].argc > 0 && segments[i].argv[0] != NULL) {
      str_copy(temp.argv0[i], sizeof(temp.argv0[i]), segments[i].argv[0]);
    }
  }

  while(true) {
    job_event_t event = job_poll_status(&temp, true);
    if(event == JOB_EVENT_CONTINUED || event == JOB_EVENT_NONE) {
      continue;
    }
    if(event == JOB_EVENT_STOPPED || event == JOB_EVENT_EXITED) {
      break;
    }
  }

  return temp.last_status;
}

static int job_wait_foreground(job_t* job) {
  if(job == NULL) {
    return 0;
  }

  job->foreground = true;
  job->background = false;
  current_job = job;

  while(true) {
    job_event_t event = job_poll_status(job, true);
    if(event == JOB_EVENT_CONTINUED || event == JOB_EVENT_NONE) {
      continue;
    }
    if(event == JOB_EVENT_STOPPED) {
      job_print_notification(job, "Stopped");
      break;
    }
    if(event == JOB_EVENT_EXITED) {
      break;
    }
  }

  current_job = NULL;

  char command_snapshot[MOSH_MAX_LINE_LENGTH];
  str_copy(command_snapshot, sizeof(command_snapshot), job->command);
  int status = (job->state == JOB_STATE_DONE) ? job_raw_status(job) : job->last_status;
  if(job->state == JOB_STATE_DONE) {
    job_release(job);
  } else {
    job->foreground = false;
  }

  jobs_poll_updates(false);
  int exit_code = decode_wait_status(status);
  if(exit_code != 0 && command_snapshot[0] != '\0') {
    write_str(STDOUT_FILENO, command_snapshot);
    write_str(STDOUT_FILENO, " exited with code ");
    char buf[16];
    format_signed_value(exit_code, buf, sizeof(buf));
    write_str(STDOUT_FILENO, buf);
    write_str(STDOUT_FILENO, "\n");
  }
  return status;
}

static int decode_wait_status(int status) {
  if(status < 0) {
    return 1;
  }
  if(WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if(WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return status;
}

static int encode_raw_status_from_code(int code) {
  if(code <= 0) {
    return 0;
  }
  return (code & 0xff) << 8;
}

static void shell_set_status_code(int code) {
  if(code < 0) {
    code = 1;
  }
  shell_last_status = code;
}

static void shell_set_status_from_raw(int status) {
  shell_last_status = decode_wait_status(status);
}

static const char* skip_spaces(const char* text) {
  while(text != NULL && (*text == ' ' || *text == '\t')) {
    text++;
  }
  return text;
}

static bool parse_job_identifier(const char* token, int* out_id) {
  if(token == NULL || out_id == NULL) {
    return false;
  }

  token = skip_spaces(token);
  if(*token == '%') {
    token++;
  }
  if(*token == '\0') {
    return false;
  }

  int value = 0;
  while(*token >= '0' && *token <= '9') {
    value = value * 10 + (*token - '0');
    token++;
  }
  if(*token != '\0' && *token != ' ' && *token != '\t') {
    return false;
  }
  if(value <= 0) {
    return false;
  }
  *out_id = value;
  return true;
}

static job_t* job_resolve_argument(const char* arguments, bool prefer_stopped) {
  const char* token = skip_spaces(arguments);
  if(token == NULL || *token == '\0') {
    job_t* job = NULL;
    if(prefer_stopped) {
      job = job_find_latest(true);
    }
    if(job == NULL) {
      job = job_find_latest(false);
    }
    return job;
  }

  char buffer[16];
  size_t len = 0;
  while(token[len] != '\0' && token[len] != ' ' && token[len] != '\t' && len + 1 < sizeof(buffer)) {
    buffer[len] = token[len];
    len++;
  }
  buffer[len] = '\0';

  int id = 0;
  if(!parse_job_identifier(buffer, &id)) {
    return NULL;
  }
  return job_find_by_id(id);
}

static size_t str_append(char* dest, size_t capacity, size_t offset, const char* src) {
  if(dest == NULL || capacity == 0 || offset >= capacity) {
    return offset;
  }
  if(src == NULL) {
    src = "";
  }
  while(*src != '\0' && offset + 1 < capacity) {
    dest[offset++] = *src++;
  }
  dest[offset] = '\0';
  return offset;
}

static void sigint_handler(int signo) {
  (void)signo;
  sigint_requested = 1;
  sigint_print_pending = 1;
}

static bool shell_take_sigint(void) {
  if(sigint_requested) {
    sigint_requested = 0;
    return true;
  }
  return false;
}

static void shell_maybe_print_sigint(void) {
  if(sigint_print_pending) {
    sigint_print_pending = 0;
    write_str(STDOUT_FILENO, "^C\n");
  }
}

static void shell_trigger_sigint(void) {
  sigint_handler(SIGINT);
}

static void sigtstp_handler(int signo) {
  (void)signo;
  sigtstp_requested = 1;
  sigtstp_print_pending = 1;
}

static bool shell_take_sigtstp(void) {
  if(sigtstp_requested) {
    sigtstp_requested = 0;
    return true;
  }
  return false;
}

static void shell_maybe_print_sigtstp(void) {
  if(sigtstp_print_pending) {
    sigtstp_print_pending = 0;
    write_str(STDOUT_FILENO, "^Z\n");
  }
}

static void shell_trigger_sigtstp(void) {
  sigtstp_handler(SIGTSTP);
}

static void shell_install_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigint_handler;
  sigaction(SIGINT, &sa, NULL);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigtstp_handler;
  sigaction(SIGTSTP, &sa, NULL);
}

#ifdef MOSH_TEST
void mosh_test_trigger_sigint(void) {
  shell_trigger_sigint();
}

void mosh_test_reset_sigint(void) {
  sigint_requested = 0;
  sigint_print_pending = 0;
}
#endif

static bool parse_command_segments(char* buffer, command_segment_t* segments, size_t* segment_count);
static int  execute_pipeline(const char* command, command_segment_t* segments, size_t segment_count, bool background);
static int  launch_pipeline(char* line);
static int  launch_command(char* line);
static size_t split_sequence(char* line, char* parts[], sequence_op_t ops[], size_t max_parts);
static char* str_find_substring(char* haystack, const char* needle);
static bool  line_replace_range(line_state_t* state, size_t start, size_t end, const char* replacement);
static void  reverse_search_reset(void);
static void  reverse_search_start(line_state_t* state);
static void  reverse_search_next(line_state_t* state);
static bool  reverse_search_handle_char(line_state_t* state, char ch);
static char* ltrim(char* text);
static void  rtrim(char* text);
static int   str_ncmp(const char* a, const char* b, size_t length);
static void  shell_var_set(const char* name, const char* value);
static const char* shell_var_get(const char* name);
static void  shell_var_unset(const char* name);
static shell_var_snapshot_t shell_var_snapshot(const char* name);
static void  shell_var_restore(const shell_var_snapshot_t* snapshot);
static void  shell_positional_push(const char* func_name, char* const argv[], size_t argc);
static void  shell_positional_pop(void);
static bool  shell_expand_variables(const char* input, char* output, size_t capacity);
static int   shell_run_command_string(const char* text);
static bool  shell_function_define_inline(const char* raw_line, int* out_status);
static bool  shell_function_define_keyword(const char* raw_args, const char* expanded_args, int* out_status);
static shell_function_t* shell_function_lookup(const char* name);
static bool  shell_function_call(const shell_function_t* fn, const char* args, int* out_status);
static bool  builtin_if(const char* raw_line, char* line, int* out_status);
static bool  builtin_while(const char* line, int* out_status);
static bool  builtin_for(const char* line, int* out_status);
static bool  builtin_set_variable(char* line, int* out_status);
static bool  builtin_export_variable(char* line, int* out_status);
static bool  builtin_unset_variable(char* line, int* out_status);
#ifdef MOSH_TEST
static bool  builtin_set_test_counter(char* line, int* out_status);
static bool  builtin_test_counter_lt(int* out_status);
#endif

static size_t debug_append_str(char* buffer, size_t pos, size_t capacity, const char* text) {
  if(text == NULL) {
    return pos;
  }
  while(*text != '\0' && pos < capacity) {
    buffer[pos++] = *text++;
  }
  return pos;
}

static size_t debug_append_hex(char* buffer, size_t pos, size_t capacity, uintptr_t value) {
  static const char digits[] = "0123456789abcdef";
  if(pos + 2 < capacity) {
    buffer[pos++] = '0';
    buffer[pos++] = 'x';
  }

  bool started = false;
  for(int shift = (int)(sizeof(uintptr_t) * 8) - 4; shift >= 0; shift -= 4) {
    unsigned nibble = (unsigned)((value >> shift) & 0xFu);
    if(nibble != 0u || started || shift == 0) {
      if(pos < capacity) {
        buffer[pos++] = digits[nibble];
      }
      started = true;
    }
  }

  return pos;
}

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

static void env_release_heap_entries(void) {
  if(process_envp == NULL) {
    memset(env_heap_flags, 0, sizeof(env_heap_flags));
    return;
  }

  for(size_t idx = 0; idx < MOSH_MAX_ENV_VARS; idx++) {
    if(process_envp[idx] == NULL) {
      break;
    }
    if(env_heap_flags[idx]) {
      free(process_envp[idx]);
      env_heap_flags[idx] = false;
    }
  }

  memset(env_heap_flags, 0, sizeof(env_heap_flags));
}

static void env_activate_storage(void) {
  if(env_storage_active) {
    return;
  }

  size_t idx = 0;
  if(process_envp != NULL) {
    for(; idx < MOSH_MAX_ENV_VARS && process_envp[idx] != NULL; idx++) {
      env_storage[idx] = process_envp[idx];
    }
  }

  if(idx >= MOSH_MAX_ENV_VARS) {
    idx = MOSH_MAX_ENV_VARS - 1;
  }

  env_storage[idx] = NULL;
  for(size_t i = idx; i < MOSH_MAX_ENV_VARS; i++) {
    env_heap_flags[i] = false;
  }

  process_envp = env_storage;
  env_storage_active = true;
}

static char** env_find_entry_slot(const char* key, size_t* index_out) {
  if(process_envp == NULL || key == NULL) {
    return NULL;
  }

  size_t key_len = str_len(key);
  if(key_len == 0) {
    return NULL;
  }

  for(size_t idx = 0; process_envp[idx] != NULL; idx++) {
    char* entry = process_envp[idx];
    size_t pos = 0;
    while(entry[pos] != '\0' && entry[pos] != '=') {
      pos++;
    }
    if(entry[pos] == '=' && pos == key_len && strncmp(entry, key, key_len) == 0) {
      if(index_out) {
        *index_out = idx;
      }
      return &process_envp[idx];
    }
  }

  return NULL;
}

static const char* env_get(const char* key) {
  char** slot = env_find_entry_slot(key, NULL);
  if(slot == NULL) {
    return NULL;
  }

  const char* entry = *slot;
  size_t key_len = str_len(key);
  if(entry[key_len] != '=') {
    return NULL;
  }
  return entry + key_len + 1;
}

#ifdef MOSH_TEST
void mosh_test_set_env(char** envp) {
  env_release_heap_entries();
  process_envp = (envp != NULL) ? envp : fallback_envp;
  env_storage_active = false;
}
#endif

static void env_set(const char* key, const char* value) {
  if(value == NULL) {
    return;
  }

  size_t index = 0;
  char** slot = env_find_entry_slot(key, &index);
  if(slot == NULL) {
    env_activate_storage();
    slot = env_find_entry_slot(key, &index);
  }
  if(slot == NULL) {
    size_t count = 0;
    while(count < MOSH_MAX_ENV_VARS && process_envp[count] != NULL) {
      count++;
    }
    if(count >= MOSH_MAX_ENV_VARS - 1) {
      write_str(STDOUT_FILENO, "mosh: export: environment full\n");
      return;
    }

    size_t key_len = str_len(key);
    size_t value_len = str_len(value);
    size_t entry_len = key_len + 1 + value_len + 1;
    char* entry = malloc(entry_len);
    if(entry == NULL) {
      return;
    }
    memcpy(entry, key, key_len);
    entry[key_len] = '=';
    memcpy(entry + key_len + 1, value, value_len);
    entry[entry_len - 1] = '\0';

    process_envp[count] = entry;
    process_envp[count + 1] = NULL;
    if(count < MOSH_MAX_ENV_VARS) {
      env_heap_flags[count] = true;
    }
    return;
  }

  char* entry = *slot;
  char* equals = strchr(entry, '=');
  if(equals == NULL) {
    return;
  }

  size_t key_len = (size_t)(equals - entry);
  size_t old_value_len = str_len(equals + 1);
  size_t new_value_len = str_len(value);

  if(new_value_len <= old_value_len) {
    memcpy(equals + 1, value, new_value_len);
    equals[1 + new_value_len] = '\0';
    return;
  }

  size_t new_entry_len = key_len + 1 + new_value_len + 1;
  char* replacement = malloc(new_entry_len);
  if(replacement == NULL) {
    return;
  }

  memcpy(replacement, entry, key_len + 1);
  memcpy(replacement + key_len + 1, value, new_value_len);
  replacement[new_entry_len - 1] = '\0';

  if(index < MOSH_MAX_ENV_VARS && env_heap_flags[index]) {
    free(entry);
  }

  *slot = replacement;
  if(index < MOSH_MAX_ENV_VARS) {
    env_heap_flags[index] = true;
  }
}

static void env_unset(const char* key) {
  if(process_envp == NULL || key == NULL || key[0] == '\0') {
    return;
  }

  env_activate_storage();

  size_t index = 0;
  char** slot = env_find_entry_slot(key, &index);
  if(slot == NULL) {
    return;
  }

  if(index < MOSH_MAX_ENV_VARS && env_heap_flags[index]) {
    free(*slot);
    env_heap_flags[index] = false;
  }

  char** envp = process_envp;
  size_t i = index;
  while(envp[i] != NULL) {
    envp[i] = envp[i + 1];
    if(i < MOSH_MAX_ENV_VARS) {
      env_heap_flags[i] = (i + 1 < MOSH_MAX_ENV_VARS) ? env_heap_flags[i + 1] : false;
    }
    i++;
  }

  if(i < MOSH_MAX_ENV_VARS) {
    env_heap_flags[i] = false;
  }
}

static bool shell_is_digit(char ch) {
  return ch >= '0' && ch <= '9';
}

static bool shell_is_alpha(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static bool shell_is_ident_start(char ch) {
  return shell_is_alpha(ch) || ch == '_';
}

static bool shell_is_ident_char(char ch) {
  return shell_is_ident_start(ch) || shell_is_digit(ch);
}

static bool shell_is_numeric_name(const char* name) {
  if(name == NULL || *name == '\0') {
    return false;
  }
  for(size_t i = 0; name[i] != '\0'; i++) {
    if(!shell_is_digit(name[i])) {
      return false;
    }
  }
  return true;
}

static bool shell_is_valid_var_name(const char* name) {
  if(name == NULL || name[0] == '\0') {
    return false;
  }
  if(shell_is_numeric_name(name)) {
    return true;
  }
  if(!shell_is_ident_start(name[0])) {
    return false;
  }
  for(size_t i = 1; name[i] != '\0'; i++) {
    if(!shell_is_ident_char(name[i])) {
      return false;
    }
  }
  return true;
}

static shell_var_t* shell_var_find(const char* name) {
  if(name == NULL) {
    return NULL;
  }
  for(size_t i = 0; i < MOSH_MAX_VARS; i++) {
    if(shell_vars[i].in_use && str_eq(shell_vars[i].name, name)) {
      return &shell_vars[i];
    }
  }
  return NULL;
}

static shell_var_t* shell_var_allocate(const char* name) {
  shell_var_t* existing = shell_var_find(name);
  if(existing != NULL) {
    return existing;
  }
  for(size_t i = 0; i < MOSH_MAX_VARS; i++) {
    if(!shell_vars[i].in_use) {
      shell_vars[i].in_use = true;
      str_copy(shell_vars[i].name, sizeof(shell_vars[i].name), name);
      shell_vars[i].value[0] = '\0';
      return &shell_vars[i];
    }
  }
  return NULL;
}

static void shell_var_set(const char* name, const char* value) {
  if(name == NULL || value == NULL) {
    return;
  }
  if(!shell_is_valid_var_name(name)) {
    return;
  }
  shell_var_t* slot = shell_var_allocate(name);
  if(slot == NULL) {
    return;
  }
  str_copy(slot->value, sizeof(slot->value), value);
}

static void shell_var_unset(const char* name) {
  if(name == NULL) {
    return;
  }
  shell_var_t* slot = shell_var_find(name);
  if(slot != NULL) {
    slot->in_use = false;
    slot->name[0] = '\0';
    slot->value[0] = '\0';
  }
}

static const char* shell_var_get(const char* name) {
  if(name == NULL || name[0] == '\0') {
    return NULL;
  }
  shell_var_t* slot = shell_var_find(name);
  if(slot != NULL) {
    return slot->value;
  }
  return env_get(name);
}

static shell_var_snapshot_t shell_var_snapshot(const char* name) {
  shell_var_snapshot_t snap;
  snap.had_value = false;
  snap.name[0] = '\0';
  snap.value[0] = '\0';
  if(name == NULL) {
    return snap;
  }
  str_copy(snap.name, sizeof(snap.name), name);
  shell_var_t* slot = shell_var_find(name);
  if(slot != NULL) {
    snap.had_value = true;
    str_copy(snap.value, sizeof(snap.value), slot->value);
  }
  return snap;
}

static void shell_var_restore(const shell_var_snapshot_t* snapshot) {
  if(snapshot == NULL || snapshot->name[0] == '\0') {
    return;
  }
  if(snapshot->had_value) {
    shell_var_set(snapshot->name, snapshot->value);
  } else {
    shell_var_unset(snapshot->name);
  }
}

static void shell_build_numeric_name(size_t value, char* out, size_t capacity) {
  if(out == NULL || capacity == 0) {
    return;
  }
  out[0] = '\0';
  format_unsigned_value(value, out, capacity);
}

static shell_var_t* shell_var_find_internal(const char* name) {
  return shell_var_find(name);
}

static void shell_positional_push(const char* func_name, char* const argv[], size_t argc) {
  if(positional_depth >= MOSH_POSITIONAL_STACK) {
    return;
  }

  positional_frame_t* frame = &positional_stack[positional_depth++];
  memset(frame, 0, sizeof(*frame));

  shell_var_t* zero = shell_var_find_internal("0");
  if(zero != NULL) {
    frame->had_zero = true;
    str_copy(frame->zero_value, sizeof(frame->zero_value), zero->value);
  }

  shell_var_t* count = shell_var_find_internal("#");
  if(count != NULL) {
    frame->had_count = true;
    str_copy(frame->count_value, sizeof(frame->count_value), count->value);
  }

  for(size_t i = 1; i <= MOSH_MAX_POSITIONAL; i++) {
    char name[8];
    shell_build_numeric_name(i, name, sizeof(name));
    shell_var_t* existing = shell_var_find_internal(name);
    frame->positional[i].had_value = (existing != NULL);
    if(existing != NULL) {
      str_copy(frame->positional[i].value, sizeof(frame->positional[i].value), existing->value);
    }
    if(i <= argc && argv != NULL && argv[i - 1] != NULL) {
      shell_var_set(name, argv[i - 1]);
    } else {
      shell_var_unset(name);
    }
  }

  char count_value[16];
  count_value[0] = '\0';
  format_unsigned_value(argc, count_value, sizeof(count_value));
  shell_var_set("#", count_value);

  if(func_name != NULL && func_name[0] != '\0') {
    shell_var_set("0", func_name);
  } else {
    shell_var_unset("0");
  }
}

static void shell_positional_pop(void) {
  if(positional_depth == 0) {
    return;
  }

  positional_frame_t* frame = &positional_stack[--positional_depth];

  for(size_t i = 1; i <= MOSH_MAX_POSITIONAL; i++) {
    char name[8];
    shell_build_numeric_name(i, name, sizeof(name));
    if(frame->positional[i].had_value) {
      shell_var_set(name, frame->positional[i].value);
    } else {
      shell_var_unset(name);
    }
  }

  if(frame->had_zero) {
    shell_var_set("0", frame->zero_value);
  } else {
    shell_var_unset("0");
  }

  if(frame->had_count) {
    shell_var_set("#", frame->count_value);
  } else {
    shell_var_unset("#");
  }
}

static bool shell_append_text(char* output, size_t capacity, size_t* index, const char* text) {
  if(output == NULL || index == NULL || text == NULL) {
    return false;
  }
  while(*text != '\0') {
    if(*index + 1 >= capacity) {
      return false;
    }
    output[(*index)++] = *text++;
  }
  output[*index] = '\0';
  return true;
}

static bool shell_expand_emit_dollar_literal(const char** cursor,
                                             char* output,
                                             size_t capacity,
                                             size_t* out_index) {
  if(output == NULL || cursor == NULL || *cursor == NULL || out_index == NULL) {
    return false;
  }
  if(*out_index + 1 >= capacity) {
    return false;
  }
  output[(*out_index)++] = '$';
  (*cursor)++;
  return true;
}

static bool shell_expand_dollar(const char** cursor,
                                char* output,
                                size_t capacity,
                                size_t* out_index) {
  if(cursor == NULL || *cursor == NULL) {
    return false;
  }

  const char* dollar = *cursor;
  write_str(STDERR_FILENO, "[mosh] dollar enter ptr=");
  debug_write_ptr(STDERR_FILENO, dollar);
  write_str(STDERR_FILENO, " next=");
  if(debug_ptr_readable(dollar)) {
    char ch = *dollar;
    char buf[4] = { ch, '\0', '\0', '\0' };
    if(ch == '\n') {
      write_str(STDERR_FILENO, "\\n");
    } else if(ch == '\r') {
      write_str(STDERR_FILENO, "\\r");
    } else {
      write_str(STDERR_FILENO, buf);
    }
  } else {
    write_str(STDERR_FILENO, "<invalid>");
  }
  write_str(STDERR_FILENO, "\n");
  if(output == NULL || out_index == NULL) {
    return false;
  }

  if(!debug_ptr_readable(dollar) || !debug_ptr_readable(dollar + 1)) {
    write_str(STDERR_FILENO, "[mosh] dollar fallback invalid cursor\n");
    return shell_expand_emit_dollar_literal(cursor, output, capacity, out_index);
  }

  char name_buffer[MOSH_MAX_VAR_NAME];
  size_t name_len = 0;
  bool overflow = false;

  const char* ptr = dollar + 1;
  char next = *ptr;
  if(next == '\0') {
    return shell_expand_emit_dollar_literal(cursor, output, capacity, out_index);
  }

  if(next == '$') {
    if(*out_index + 1 >= capacity) {
      return false;
    }
    output[(*out_index)++] = '$';
    *cursor = ptr + 1;
    return true;
  }

  if(next == '{') {
    ptr++;
    while(*ptr != '\0' && *ptr != '}') {
      if(name_len + 1 >= sizeof(name_buffer)) {
        overflow = true;
      } else {
        name_buffer[name_len++] = *ptr;
      }
      ptr++;
    }

    if(*ptr != '}') {
      return shell_expand_emit_dollar_literal(cursor, output, capacity, out_index);
    }

    if(overflow || name_len == 0) {
      return shell_expand_emit_dollar_literal(cursor, output, capacity, out_index);
    }

    name_buffer[name_len] = '\0';
    const char* value = shell_var_get(name_buffer);
    if(value == NULL) {
      value = "";
    }
    if(!shell_append_text(output, capacity, out_index, value)) {
      return false;
    }
    *cursor = ptr + 1;
    return true;
  }

  if(shell_is_digit(next)) {
    while(shell_is_digit(*ptr)) {
      if(name_len + 1 >= sizeof(name_buffer)) {
        overflow = true;
      } else {
        name_buffer[name_len++] = *ptr;
      }
      ptr++;
    }
  } else if(shell_is_ident_start(next)) {
    while(shell_is_ident_char(*ptr)) {
      if(name_len + 1 >= sizeof(name_buffer)) {
        overflow = true;
      } else {
        name_buffer[name_len++] = *ptr;
      }
      ptr++;
    }
  } else {
    return shell_expand_emit_dollar_literal(cursor, output, capacity, out_index);
  }

  if(name_len == 0 || overflow) {
    return shell_expand_emit_dollar_literal(cursor, output, capacity, out_index);
  }

  name_buffer[name_len] = '\0';
  const char* value = shell_var_get(name_buffer);
  if(value == NULL) {
    value = "";
  }
  if(!shell_append_text(output, capacity, out_index, value)) {
    return false;
  }
  *cursor = ptr;
  return true;
}

static bool shell_expand_variables(const char* input, char* output, size_t capacity) {
  if(output == NULL || capacity == 0) {
    return false;
  }
  if(input == NULL) {
    output[0] = '\0';
    return true;
  }

  debug_log_expand_start(input, output, capacity);

  size_t out_index = 0;
  output[0] = '\0';

  const char* cursor = input;
  while(true) {
    if(!debug_ptr_readable(cursor)) {
      write_str(STDERR_FILENO, "[mosh] expand invalid cursor=");
      debug_write_ptr(STDERR_FILENO, cursor);
      write_str(STDERR_FILENO, "\n");
      return false;
    }
    char ch = *cursor;
    write_str(STDERR_FILENO, "[mosh] expand char ptr=");
    debug_write_ptr(STDERR_FILENO, cursor);
    write_str(STDERR_FILENO, " ch=");
    char display[5];
    display[0] = ch;
    display[1] = '\0';
    if(ch == '\n') {
      write_str(STDERR_FILENO, "\\n");
    } else if(ch == '\r') {
      write_str(STDERR_FILENO, "\\r");
    } else {
      write_str(STDERR_FILENO, display);
    }
    write_str(STDERR_FILENO, "\n");
    if(ch == '\0') {
      break;
    }

    if(ch == '$') {
      write_str(STDERR_FILENO, "[mosh] dollar guard cursor=");
      debug_write_ptr(STDERR_FILENO, cursor);
      write_str(STDERR_FILENO, " next=");
      const char* next_ptr = cursor + 1;
      debug_write_ptr(STDERR_FILENO, next_ptr);
      write_str(STDERR_FILENO, " ch=");
      if(debug_ptr_readable(cursor)) {
        char current = *cursor;
        if(current == '\n') {
          write_str(STDERR_FILENO, "\\n");
        } else if(current == '\r') {
          write_str(STDERR_FILENO, "\\r");
        } else {
          char buf[2] = { current, '\0' };
          write_str(STDERR_FILENO, buf);
        }
      } else {
        write_str(STDERR_FILENO, "<invalid>");
      }
      write_str(STDERR_FILENO, " next_ch=");
      if(debug_ptr_readable(next_ptr)) {
        char next_ch = *next_ptr;
        if(next_ch == '\n') {
          write_str(STDERR_FILENO, "\\n");
        } else if(next_ch == '\r') {
          write_str(STDERR_FILENO, "\\r");
        } else {
          char buf[2] = { next_ch, '\0' };
          write_str(STDERR_FILENO, buf);
        }
      } else {
        write_str(STDERR_FILENO, "<invalid>");
      }
      write_str(STDERR_FILENO, "\n");

      if(!debug_ptr_readable(cursor) || !debug_ptr_readable(next_ptr)) {
        write_str(STDERR_FILENO, "[mosh] dollar guard treating literal due to invalid pointer\n");
        if(out_index + 1 >= capacity) {
          return false;
        }
        output[out_index++] = '$';
        cursor++;
        continue;
      }

      debug_log_expand_dollar(cursor);
      if(!shell_expand_dollar(&cursor, output, capacity, &out_index)) {
        return false;
      }
      continue;
    }

    if(ch == '\\' && cursor[1] != '\0') {
      cursor++;
      ch = *cursor;
    }

    if(out_index + 1 >= capacity) {
      return false;
    }
    output[out_index++] = ch;
    cursor++;
  }

  if(out_index >= capacity) {
    return false;
  }
  output[out_index] = '\0';
  return true;
}

static int shell_run_command_string(const char* text) {
  if(text == NULL) {
    shell_set_status_code(0);
    return 0;
  }

  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), text);
  return launch_command(buffer);
}

static void shell_finish_builtin_code(int code, int* out_status) {
  shell_set_status_code(code);
  if(out_status != NULL) {
    *out_status = encode_raw_status_from_code(code);
  }
}

static void shell_finish_builtin_raw(int raw_status, int* out_status) {
  shell_set_status_from_raw(raw_status);
  if(out_status != NULL) {
    *out_status = raw_status;
  }
}

static bool compute_timespec_diff(const struct timespec* start,
                                  const struct timespec* end,
                                  long long* out_seconds,
                                  long long* out_nanoseconds) {
  if(start == NULL || end == NULL || out_seconds == NULL || out_nanoseconds == NULL) {
    return false;
  }

  const long long ns_per_sec = 1000000000LL;
  long long seconds = (long long)end->tv_sec - (long long)start->tv_sec;
  long long nanoseconds = (long long)end->tv_nsec - (long long)start->tv_nsec;

  if(nanoseconds < 0) {
    nanoseconds += ns_per_sec;
    seconds -= 1;
  }

  if(seconds < 0) {
    seconds = 0;
    nanoseconds = 0;
  }

  if(nanoseconds < 0) {
    nanoseconds = 0;
  }

  *out_seconds = seconds;
  *out_nanoseconds = nanoseconds;
  return true;
}

static void print_duration_line(const char* label,
                                long long seconds,
                                long long nanoseconds) {
  if(label == NULL) {
    label = "";
  }
  if(seconds < 0) {
    seconds = 0;
  }
  if(nanoseconds < 0) {
    nanoseconds = 0;
  }

  const long long ns_per_millisecond = 1000000LL;
  long long fractional = nanoseconds / ns_per_millisecond;
  if(fractional < 0) {
    fractional = 0;
  }
  if(fractional > 999) {
    fractional = 999;
  }

  char buffer[64];
  int written = snprintf(buffer, sizeof(buffer), "%-4s %lld.%03llds\n", label, seconds, fractional);
  if(written <= 0) {
    return;
  }

  size_t to_write = (written < (int)sizeof(buffer)) ? (size_t)written : sizeof(buffer) - 1;
  write_bytes(STDERR_FILENO, buffer, to_write);
}

static bool shell_is_token_boundary(char ch) {
  return ch == '\0' || ch == ' ' || ch == '\t' || ch == ';' || ch == '\n' || ch == '{' || ch == '}';
}

static char* shell_find_keyword(char* text, const char* keyword) {
  if(text == NULL || keyword == NULL || keyword[0] == '\0') {
    return NULL;
  }
  size_t len = str_len(keyword);
  for(char* cursor = text; *cursor != '\0'; cursor++) {
    if(str_ncmp(cursor, keyword, len) != 0) {
      continue;
    }
    char prev = (cursor == text) ? ' ' : cursor[-1];
    char next = cursor[len];
    if(shell_is_token_boundary(prev) && shell_is_token_boundary(next)) {
      return cursor;
    }
  }
  return NULL;
}

static void shell_trim_trailing_semicolon(char* text) {
  if(text == NULL) {
    return;
  }
  rtrim(text);
  size_t len = str_len(text);
  if(len > 0 && text[len - 1] == ';') {
    text[len - 1] = '\0';
    rtrim(text);
  }
}

static char* shell_find_char_reverse(char* text, char target) {
  if(text == NULL) {
    return NULL;
  }
  size_t len = str_len(text);
  while(len > 0) {
    len--;
    if(text[len] == target) {
      return &text[len];
    }
  }
  return NULL;
}

static bool shell_extract_braced_block(char* open_brace, char** body_out, char** rest_out) {
  if(open_brace == NULL) {
    return false;
  }
  while(*open_brace != '\0' && *open_brace != '{') {
    open_brace++;
  }
  if(*open_brace != '{') {
    return false;
  }

  char* cursor = open_brace + 1;
  int depth = 1;
  while(*cursor != '\0') {
    if(*cursor == '{') {
      depth++;
    } else if(*cursor == '}') {
      depth--;
      if(depth == 0) {
        *cursor = '\0';
        *open_brace = '\0';
        if(body_out != NULL) {
          char* body = ltrim(open_brace + 1);
          rtrim(body);
          *body_out = body;
        }
        if(rest_out != NULL) {
          *rest_out = cursor + 1;
        }
        return true;
      }
    }
    cursor++;
  }
  return false;
}

static bool shell_starts_with_keyword(const char* text, const char* keyword) {
  if(text == NULL || keyword == NULL) {
    return false;
  }
  size_t len = str_len(keyword);
  if(str_ncmp(text, keyword, len) != 0) {
    return false;
  }
  return shell_is_token_boundary(text[len]);
}

static bool shell_is_inline_function_signature(const char* text) {
  if(text == NULL) {
    return false;
  }
  size_t idx = 0;
  if(!shell_is_ident_start(text[idx])) {
    return false;
  }
  while(shell_is_ident_char(text[idx])) {
    idx++;
  }
  while(text[idx] == ' ' || text[idx] == '\t') {
    idx++;
  }
  if(text[idx] != '(' || text[idx + 1] != ')') {
    return false;
  }
  idx += 2;
  while(text[idx] == ' ' || text[idx] == '\t') {
    idx++;
  }
  return text[idx] == '{';
}

static int shell_parse_int(const char* text) {
  if(text == NULL || *text == '\0') {
    return 0;
  }
  int sign = 1;
  size_t idx = 0;
  if(text[idx] == '+') {
    idx++;
  } else if(text[idx] == '-') {
    sign = -1;
    idx++;
  }
  int value = 0;
  while(shell_is_digit(text[idx])) {
    value = value * 10 + (text[idx] - '0');
    idx++;
  }
  return sign * value;
}

static shell_function_t* shell_function_allocate(const char* name) {
  if(name == NULL) {
    return NULL;
  }
  shell_function_t* existing = shell_function_lookup(name);
  if(existing != NULL) {
    return existing;
  }
  for(size_t i = 0; i < MOSH_MAX_FUNCTIONS; i++) {
    if(!shell_functions[i].in_use) {
      shell_functions[i].in_use = true;
      str_copy(shell_functions[i].name, sizeof(shell_functions[i].name), name);
      shell_functions[i].body[0] = '\0';
      return &shell_functions[i];
    }
  }
  return NULL;
}

static shell_function_t* shell_function_lookup(const char* name) {
  if(name == NULL) {
    return NULL;
  }
  for(size_t i = 0; i < MOSH_MAX_FUNCTIONS; i++) {
    if(shell_functions[i].in_use && str_eq(shell_functions[i].name, name)) {
      return &shell_functions[i];
    }
  }
  return NULL;
}

static bool shell_function_define_internal(const char* name, const char* body, int* out_status) {
  if(name == NULL || body == NULL) {
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  if(!shell_is_valid_var_name(name)) {
    write_str(STDOUT_FILENO, "mosh: function: invalid name\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  shell_function_t* slot = shell_function_allocate(name);
  if(slot == NULL) {
    write_str(STDOUT_FILENO, "mosh: function: table full\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  char body_copy[MOSH_MAX_FUNCTION_BODY];
  str_copy(body_copy, sizeof(body_copy), body);
  char* trimmed_body = ltrim(body_copy);
  shell_trim_trailing_semicolon(trimmed_body);
  str_copy(slot->body, sizeof(slot->body), trimmed_body);
  str_copy(slot->name, sizeof(slot->name), name);
  shell_finish_builtin_code(0, out_status);
  return true;
}

static bool shell_function_define_inline(const char* raw_line, int* out_status) {
  if(raw_line == NULL) {
    return false;
  }
  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), raw_line);
  char* cursor = buffer;
  cursor = (char*)skip_spaces(cursor);
  char* name_start = cursor;
  while(shell_is_ident_char(*cursor)) {
    cursor++;
  }
  if(cursor == name_start) {
    return false;
  }
  char saved = *cursor;
  *cursor = '\0';
  char* name = name_start;
  if(saved != '(') {
    *cursor = saved;
    return false;
  }
  cursor++;
  if(cursor[0] != ')') {
    *cursor = saved;
    return false;
  }
  cursor++;
  cursor = (char*)skip_spaces(cursor);
  if(cursor[0] != '{') {
    *cursor = saved;
    return false;
  }
  cursor++;
  char* body_start = cursor;
  char* closing = shell_find_char_reverse(body_start, '}');
  if(closing == NULL) {
    write_str(STDOUT_FILENO, "mosh: function: missing closing '}\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  *closing = '\0';
  shell_trim_trailing_semicolon(body_start);
  return shell_function_define_internal(name, body_start, out_status);
}

static bool shell_function_define_keyword(const char* raw_args, const char* expanded_args, int* out_status) {
  (void)expanded_args;
  if(raw_args == NULL) {
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), raw_args);
  char* cursor = buffer;
  cursor = (char*)skip_spaces(cursor);
  char* name_start = cursor;
  while(shell_is_ident_char(*cursor)) {
    cursor++;
  }
  if(cursor == name_start) {
    write_str(STDOUT_FILENO, "mosh: function: missing name\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  char saved = *cursor;
  *cursor = '\0';
  char* name = name_start;
  cursor++;
  cursor = (char*)skip_spaces(cursor);
  if(saved == '(') {
    if(cursor[0] != ')') {
      write_str(STDOUT_FILENO, "mosh: function: malformed definition\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }
    cursor++;
    cursor = (char*)skip_spaces(cursor);
  }
  if(cursor[0] != '{') {
    write_str(STDOUT_FILENO, "mosh: function: expected '{'\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  cursor++;
  char* body_start = cursor;
  char* closing = shell_find_char_reverse(body_start, '}');
  if(closing == NULL) {
    write_str(STDOUT_FILENO, "mosh: function: missing closing '}'\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  *closing = '\0';
  shell_trim_trailing_semicolon(body_start);
  return shell_function_define_internal(name, body_start, out_status);
}

static bool shell_function_call(const shell_function_t* fn, const char* args, int* out_status) {
  if(fn == NULL) {
    return false;
  }

  char args_copy[MOSH_MAX_FUNCTION_BODY];
  str_copy(args_copy, sizeof(args_copy), args != NULL ? args : "");

  char* argv[MOSH_MAX_ARGS];
  size_t argc = 0;
  char* cursor = args_copy;
  while(*cursor != '\0' && argc < MOSH_MAX_ARGS) {
    cursor = (char*)skip_spaces(cursor);
    if(*cursor == '\0') {
      break;
    }
    argv[argc++] = cursor;
    while(*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
      cursor++;
    }
    if(*cursor != '\0') {
      *cursor++ = '\0';
    }
  }

  shell_positional_push(fn->name, argv, argc);
  int status = shell_run_command_string(fn->body);
  shell_positional_pop();
  shell_finish_builtin_raw(status, out_status);
  return true;
}

static size_t shell_split_words(char* text, char* tokens[], size_t max_tokens) {
  size_t count = 0;
  if(text == NULL || tokens == NULL || max_tokens == 0) {
    return 0;
  }
  char* cursor = text;
  while(*cursor != '\0' && count < max_tokens) {
    cursor = (char*)skip_spaces(cursor);
    if(*cursor == '\0') {
      break;
    }
    tokens[count++] = cursor;
    while(*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
      cursor++;
    }
    if(*cursor != '\0') {
      *cursor++ = '\0';
    }
  }
  return count;
}

static bool builtin_set_variable(char* line, int* out_status) {
  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), line != NULL ? line : "");

  char* tokens[3];
  size_t count = shell_split_words(buffer, tokens, 3);

  if(count == 0) {
    for(size_t i = 0; i < MOSH_MAX_VARS; i++) {
      if(shell_vars[i].in_use) {
        write_str(STDOUT_FILENO, shell_vars[i].name);
        write_str(STDOUT_FILENO, "=");
        write_str(STDOUT_FILENO, shell_vars[i].value);
        write_str(STDOUT_FILENO, "\n");
      }
    }
    shell_finish_builtin_code(0, out_status);
    return true;
  }

  const char* name = tokens[0];
  const char* value = "";
  char value_buffer[MOSH_MAX_FUNCTION_BODY];

  char* equals = strchr(tokens[0], '=');
  if(equals != NULL) {
    *equals = '\0';
    value = equals + 1;
    if(count > 1) {
      write_str(STDOUT_FILENO, "mosh: set: too many arguments\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }
  } else {
    if(count >= 2) {
      value_buffer[0] = '\0';
      str_copy(value_buffer, sizeof(value_buffer), tokens[1]);
      for(size_t i = 2; i < count; i++) {
        size_t len = str_len(value_buffer);
        if(len + 1 < sizeof(value_buffer)) {
          value_buffer[len++] = ' ';
          value_buffer[len] = '\0';
        }
        str_copy(&value_buffer[str_len(value_buffer)], sizeof(value_buffer) - str_len(value_buffer), tokens[i]);
      }
      value = value_buffer;
    }
  }

  if(!shell_is_valid_var_name(name)) {
    write_str(STDOUT_FILENO, "mosh: set: invalid variable name\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char normalized[MOSH_MAX_FUNCTION_BODY];
  const char* trimmed_value = (const char*)skip_spaces(value);
  size_t trimmed_len = str_len(trimmed_value);
  if(trimmed_len >= 2 && trimmed_value[0] == '[' && trimmed_value[trimmed_len - 1] == ']') {
    char list_copy[MOSH_MAX_FUNCTION_BODY];
    str_copy(list_copy, sizeof(list_copy), trimmed_value + 1);
    char* end = shell_find_char_reverse(list_copy, ']');
    if(end != NULL) {
      *end = '\0';
    }
    char* entry = list_copy;
    normalized[0] = '\0';
    size_t norm_index = 0;
    while(*entry != '\0') {
      entry = (char*)skip_spaces(entry);
      if(*entry == '\0') {
        break;
      }
      char* sep = entry;
      while(*sep != '\0' && *sep != ',') {
        sep++;
      }
      char saved = *sep;
      *sep = '\0';
      char expanded[MOSH_MAX_FUNCTION_BODY];
      if(!shell_expand_variables(entry, expanded, sizeof(expanded))) {
        write_str(STDOUT_FILENO, "mosh: set: expansion too long\n");
        shell_finish_builtin_code(1, out_status);
        return true;
      }
      char expanded_copy[MOSH_MAX_FUNCTION_BODY];
      str_copy(expanded_copy, sizeof(expanded_copy), expanded);
      char* words[64];
      size_t count = shell_split_words(expanded_copy, words, 64);
      for(size_t j = 0; j < count; j++) {
        if(norm_index != 0) {
          if(norm_index + 1 >= sizeof(normalized)) {
            write_str(STDOUT_FILENO, "mosh: set: value too long\n");
            shell_finish_builtin_code(1, out_status);
            return true;
          }
          normalized[norm_index++] = ' ';
        }
        size_t word_len = str_len(words[j]);
        if(norm_index + word_len >= sizeof(normalized)) {
          write_str(STDOUT_FILENO, "mosh: set: value too long\n");
          shell_finish_builtin_code(1, out_status);
          return true;
        }
        memcpy(&normalized[norm_index], words[j], word_len);
        norm_index += word_len;
        normalized[norm_index] = '\0';
      }
      *sep = saved;
      if(saved == ',') {
        entry = sep + 1;
      } else {
        break;
      }
    }
    value = normalized;
  }

  shell_var_set(name, value);
  shell_finish_builtin_code(0, out_status);
  return true;
}

static bool builtin_export_variable(char* line, int* out_status) {
  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), line != NULL ? line : "");

  char* tokens[32];
  size_t count = shell_split_words(buffer, tokens, 32);

  if(count == 0) {
    char** envp = (process_envp != NULL) ? process_envp : fallback_envp;
    for(size_t i = 0; envp != NULL && envp[i] != NULL; i++) {
      write_str(STDOUT_FILENO, envp[i]);
      write_char_stdout('\n');
    }
    shell_finish_builtin_code(0, out_status);
    return true;
  }

  for(size_t i = 0; i < count; i++) {
    char* entry = tokens[i];
    if(entry == NULL || entry[0] == '\0') {
      continue;
    }

    char* equals = strchr(entry, '=');
    char* name = entry;
    const char* value = NULL;

    if(equals != NULL) {
      *equals = '\0';
      value = equals + 1;
    } else {
      value = shell_var_get(name);
      if(value == NULL) {
        value = env_get(name);
      }
      if(value == NULL) {
        value = "";
      }
    }

    if(!shell_is_valid_var_name(name) || shell_is_numeric_name(name)) {
      write_str(STDOUT_FILENO, "mosh: export: invalid name\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }

    shell_var_set(name, value);
    env_set(name, value);
  }

  shell_finish_builtin_code(0, out_status);
  return true;
}

static bool builtin_unset_variable(char* line, int* out_status) {
  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), line != NULL ? line : "");
  char* tokens[2];
  size_t count = shell_split_words(buffer, tokens, 2);
  if(count == 0) {
    shell_finish_builtin_code(0, out_status);
    return true;
  }
  if(count > 1) {
    write_str(STDOUT_FILENO, "mosh: unset: too many arguments\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  const char* name = tokens[0];
  bool numeric = shell_is_numeric_name(name);
  if(!numeric && !shell_is_valid_var_name(name)) {
    write_str(STDOUT_FILENO, "mosh: unset: invalid name\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  if(!numeric) {
    env_unset(name);
  }
  shell_var_unset(name);
  shell_finish_builtin_code(0, out_status);
  return true;
}

static bool builtin_if(const char* raw_line, char* line, int* out_status) {
  (void)line;
  if(raw_line == NULL) {
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), raw_line);

  char* trimmed = ltrim(buffer);
  if(shell_starts_with_keyword(trimmed, "if")) {
    trimmed = (char*)skip_spaces(trimmed + 2);
  }

  char* first_brace = strchr(trimmed, '{');
  if(first_brace == NULL) {
    write_str(STDOUT_FILENO, "mosh: if: expected '{'\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char saved = *first_brace;
  *first_brace = '\0';
  char* condition = trimmed;
  rtrim(condition);
  *first_brace = saved;
  shell_trim_trailing_semicolon(condition);
  if(condition[0] == '\0') {
    write_str(STDOUT_FILENO, "mosh: if: empty condition\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char* then_body = NULL;
  char* rest = NULL;
  if(!shell_extract_braced_block(first_brace, &then_body, &rest)) {
    write_str(STDOUT_FILENO, "mosh: if: malformed then block\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char* else_body = NULL;
  rest = (rest != NULL) ? (char*)skip_spaces(rest) : NULL;
  if(rest != NULL && shell_starts_with_keyword(rest, "else")) {
    rest = (char*)skip_spaces(rest + 4);
    if(!shell_extract_braced_block(rest, &else_body, &rest)) {
      write_str(STDOUT_FILENO, "mosh: if: malformed else block\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }
    rest = (rest != NULL) ? (char*)skip_spaces(rest) : NULL;
  }

  if(rest != NULL && *rest != '\0') {
    write_str(STDOUT_FILENO, "mosh: if: unexpected text after block\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  int cond_status = shell_run_command_string(condition);
  int cond_code = decode_wait_status(cond_status);
  if(cond_code == 0) {
    if(then_body[0] == '\0') {
      shell_finish_builtin_raw(cond_status, out_status);
      return true;
    }
    int then_status = shell_run_command_string(then_body);
    shell_finish_builtin_raw(then_status, out_status);
    return true;
  }

  if(else_body != NULL && else_body[0] != '\0') {
    int else_status = shell_run_command_string(else_body);
    shell_finish_builtin_raw(else_status, out_status);
    return true;
  }

  shell_finish_builtin_raw(cond_status, out_status);
  return true;
}

static bool builtin_while(const char* line, int* out_status) {
  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), line != NULL ? line : "");

  char* trimmed = ltrim(buffer);
  if(shell_starts_with_keyword(trimmed, "while")) {
    trimmed = (char*)skip_spaces(trimmed + 5);
  }

  char* brace = strchr(trimmed, '{');
  if(brace == NULL) {
    write_str(STDOUT_FILENO, "mosh: while: expected '{'\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char saved = *brace;
  *brace = '\0';
  char* condition = trimmed;
  rtrim(condition);
  *brace = saved;
  shell_trim_trailing_semicolon(condition);
  if(condition[0] == '\0') {
    write_str(STDOUT_FILENO, "mosh: while: empty condition\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char* body = NULL;
  char* rest = NULL;
  if(!shell_extract_braced_block(brace, &body, &rest)) {
    write_str(STDOUT_FILENO, "mosh: while: malformed body\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  if(rest != NULL && *skip_spaces(rest) != '\0') {
    write_str(STDOUT_FILENO, "mosh: while: unexpected text after block\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  int last_status = encode_raw_status_from_code(0);
  while(true) {
#ifdef MOSH_TEST
    write_str(STDOUT_FILENO, "[debug while] entering\n");
#endif
    int cond_status = shell_run_command_string(condition);
    int cond_code = decode_wait_status(cond_status);
    if(cond_code != 0) {
      last_status = cond_status;
      break;
    }
    if(body[0] == '\0') {
      last_status = cond_status;
      break;
    }
    last_status = shell_run_command_string(body);
  }

  shell_finish_builtin_raw(last_status, out_status);
  return true;
}

static bool builtin_for(const char* line, int* out_status) {
  char buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(buffer, sizeof(buffer), line != NULL ? line : "");

  char* trimmed = ltrim(buffer);
  if(shell_starts_with_keyword(trimmed, "for")) {
    trimmed = (char*)skip_spaces(trimmed + 3);
  }

  char* cursor = trimmed;
  while(shell_is_ident_char(*cursor)) {
    cursor++;
  }
  if(cursor == trimmed) {
    write_str(STDOUT_FILENO, "mosh: for: missing variable name\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char saved = *cursor;
  *cursor = '\0';
  char* var_name = trimmed;
  if(!shell_is_valid_var_name(var_name)) {
    write_str(STDOUT_FILENO, "mosh: for: invalid variable name\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  *cursor = saved;
  cursor = (saved == '\0') ? cursor : cursor + 1;
  cursor = (char*)skip_spaces(cursor);

  if(str_ncmp(cursor, "in", 2) != 0) {
    write_str(STDOUT_FILENO, "mosh: for: missing 'in'\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  cursor += 2;
  cursor = (char*)skip_spaces(cursor);

  char* brace = strchr(cursor, '{');
  if(brace == NULL) {
    write_str(STDOUT_FILENO, "mosh: for: expected '{'\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char list_expr[MOSH_MAX_FUNCTION_BODY];
  size_t list_len = (size_t)(brace - cursor);
  if(list_len >= sizeof(list_expr)) {
    write_str(STDOUT_FILENO, "mosh: for: list too long\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  memcpy(list_expr, cursor, list_len);
  list_expr[list_len] = '\0';
  rtrim(list_expr);
  shell_trim_trailing_semicolon(list_expr);

  char* body = NULL;
  char* rest = NULL;
  if(!shell_extract_braced_block(brace, &body, &rest)) {
    write_str(STDOUT_FILENO, "mosh: for: malformed body\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  if(rest != NULL && *skip_spaces(rest) != '\0') {
    write_str(STDOUT_FILENO, "mosh: for: unexpected text after block\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }

  char list_buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(list_buffer, sizeof(list_buffer), list_expr);

  char value_storage[128][MOSH_MAX_VAR_VALUE];
  char* values[128];
  size_t value_count = 0;

  char* item = list_buffer;
  size_t len = str_len(list_buffer);
  if(len >= 2 && list_buffer[0] == '[' && list_buffer[len - 1] == ']') {
    list_buffer[len - 1] = '\0';
    item = list_buffer + 1;
  }

  while(*item != '\0') {
    item = (char*)skip_spaces(item);
    if(*item == '\0') {
      break;
    }
    char* sep = item;
    while(*sep != '\0' && *sep != ',') {
      sep++;
    }
    char saved_sep = *sep;
    *sep = '\0';

    char expanded[MOSH_MAX_FUNCTION_BODY];
    if(!shell_expand_variables(item, expanded, sizeof(expanded))) {
      write_str(STDOUT_FILENO, "mosh: for: expansion too long\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }

    char expanded_copy[MOSH_MAX_FUNCTION_BODY];
    str_copy(expanded_copy, sizeof(expanded_copy), expanded);
    char* words[64];
    size_t word_count = shell_split_words(expanded_copy, words, 64);
    for(size_t j = 0; j < word_count && value_count < sizeof(values) / sizeof(values[0]); j++) {
      str_copy(value_storage[value_count], sizeof(value_storage[value_count]), words[j]);
      values[value_count] = value_storage[value_count];
      value_count++;
    }

    *sep = saved_sep;
    if(saved_sep == ',') {
      item = sep + 1;
    } else {
      break;
    }
  }

  shell_var_snapshot_t snapshot = shell_var_snapshot(var_name);
  int last_status = encode_raw_status_from_code(0);

  for(size_t i = 0; i < value_count; i++) {
    shell_var_set(var_name, values[i]);
    last_status = shell_run_command_string(body);
  }

  shell_var_restore(&snapshot);
  shell_finish_builtin_raw(last_status, out_status);
  return true;
}

#ifdef MOSH_TEST
static bool builtin_set_test_counter(char* line, int* out_status) {
  char buffer[64];
  str_copy(buffer, sizeof(buffer), line != NULL ? line : "");
  char* tokens[2];
  size_t count = shell_split_words(buffer, tokens, 2);
  if(count == 0) {
    test_counter_limit = 0;
    test_counter_value = 0;
    shell_finish_builtin_code(0, out_status);
    return true;
  }
  if(count > 1) {
    write_str(STDOUT_FILENO, "__test_set_counter expects a single integer\n");
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  int limit = shell_parse_int(tokens[0]);
  if(limit < 0) {
    limit = 0;
  }
  test_counter_limit = limit;
  test_counter_value = 0;
  shell_finish_builtin_code(0, out_status);
  return true;
}

static bool builtin_test_counter_lt(int* out_status) {
#ifdef MOSH_TEST
  write_str(STDOUT_FILENO, "[debug counter]\n");
#endif
  if(test_counter_value < test_counter_limit) {
    test_counter_value++;
    shell_finish_builtin_code(0, out_status);
  } else {
    shell_finish_builtin_code(1, out_status);
  }
  return true;
}
#endif

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

static bool strip_background_marker(char* text) {
  if(text == NULL) {
    return false;
  }

  size_t len = str_len(text);
  while(len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
    text[--len] = '\0';
  }

  if(len > 0 && text[len - 1] == '&') {
    text[--len] = '\0';
    while(len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
      text[--len] = '\0';
    }
    return true;
  }

  return false;
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
  segment->redirect_err = NULL;
  segment->redirect_out_append = false;
  segment->redirect_err_append = false;
  segment->redirect_err_to_stdout = false;
  segment->redirect_out_to_stderr = false;
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
      if(i + 1 < token_count && tokens[i + 1][0] == '<' && tokens[i + 1][1] == '\0') {
        return false;
      }
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
      int target_fd = 1;
      bool append = false;
      bool both_streams = false;

      if(i > 0) {
        char* prev = tokens[i - 1];
        if(prev != NULL && prev[0] != '\0') {
          if(prev[0] >= '0' && prev[0] <= '9' && prev[1] == '\0') {
            target_fd = prev[0] - '0';
            if(current->argc > 0 && current->argv[current->argc - 1] == prev) {
              current->argc--;
              current->argv[current->argc] = NULL;
            }
          } else if(prev[0] == '&' && prev[1] == '\0') {
            both_streams = true;
            if(current->argc > 0 && current->argv[current->argc - 1] == prev) {
              current->argc--;
              current->argv[current->argc] = NULL;
            }
          }
        }
      }

      if(i + 1 < token_count && tokens[i + 1][0] == '>' && tokens[i + 1][1] == '\0') {
        append = true;
        i++;
      }

      if(i + 1 >= token_count) {
        return false;
      }

      char* destination = tokens[++i];

      if(destination[0] == '&') {
        const char* fd_spec = destination + 1;
        if(fd_spec[0] == '\0') {
          if(i + 1 >= token_count) {
            return false;
          }
          destination = tokens[++i];
          fd_spec = destination;
        }

        if(fd_spec[0] == '-' && fd_spec[1] == '\0') {
          return false;
        }

        if(fd_spec[0] < '0' || fd_spec[0] > '9' || fd_spec[1] != '\0') {
          return false;
        }

        int dup_target = fd_spec[0] - '0';

        if(both_streams) {
          return false;
        }

        if(target_fd == 1) {
          if(dup_target == 2) {
            current->redirect_out_to_stderr = true;
            continue;
          }
          if(dup_target == 1) {
            continue;
          }
        } else if(target_fd == 2) {
          if(dup_target == 1) {
            if(current->redirect_err != NULL || current->redirect_err_to_stdout) {
              return false;
            }
            current->redirect_err_to_stdout = true;
            continue;
          }
          if(dup_target == 2) {
            continue;
          }
        }

        return false;
      }

      if(both_streams) {
        if(current->redirect_out != NULL || current->redirect_err != NULL) {
          return false;
        }
        current->redirect_out = destination;
        current->redirect_err = destination;
        current->redirect_out_append = append;
        current->redirect_err_append = append;
        continue;
      }

      if(target_fd == 1) {
        if(current->redirect_out != NULL) {
          return false;
        }
        current->redirect_out = destination;
        current->redirect_out_append = append;
        continue;
      }

      if(target_fd == 2) {
        if(current->redirect_err != NULL) {
          return false;
        }
        current->redirect_err = destination;
        current->redirect_err_append = append;
        continue;
      }

      return false;
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

#ifdef MOSH_TEST
bool mosh_test_parse_pipeline(const char* line,
                              command_segment_t* segments,
                              size_t* segment_count) {
  if(line == NULL || segments == NULL || segment_count == NULL) {
    return false;
  }

  char working[MOSH_MAX_LINE_LENGTH];
  str_copy(working, sizeof(working), line);

  strip_background_marker(working);

  char expanded[MOSH_MAX_LINE_LENGTH * 3];
  size_t idx = 0;
  for(size_t i = 0; working[i] != '\0' && idx + 3 < sizeof(expanded); i++) {
    char ch = working[i];
    if(ch == '|' || ch == '<' || ch == '>') {
      expanded[idx++] = ' ';
      expanded[idx++] = ch;
      expanded[idx++] = ' ';
    } else {
      expanded[idx++] = ch;
    }
  }
  expanded[idx] = '\0';

  for(size_t i = 0; i < MOSH_MAX_SEGMENTS; i++) {
    init_segment(&segments[i]);
  }

  return parse_command_segments(expanded, segments, segment_count);
}

const char* mosh_test_env_get(const char* key) {
  return env_get(key);
}
#endif

static inline void close_fd_if_needed(int fd) {
  if(fd >= 0) {
    close(fd);
  }
}

static int job_raw_status(const job_t* job) {
  if(job == NULL || job->segment_count == 0) {
    return 0;
  }
  return job->statuses[job->segment_count - 1];
}

static void job_handle_exit_messages(job_t* job) {
  if(job == NULL || job->segment_count == 0) {
    return;
  }

  int status = job_raw_status(job);
  if(WIFEXITED(status)) {
    int code = WEXITSTATUS(status);
    if(code == 127) {
      if(job->argv0[job->segment_count - 1][0] != '\0') {
        const char prefix[] = "mosh: command not found: ";
        write_bytes(STDOUT_FILENO, prefix, sizeof(prefix) - 1);
        write_str(STDOUT_FILENO, job->argv0[job->segment_count - 1]);
        write_char_stdout('\n');
      }
      return;
    }
    if(code > 0 && code != 130) {
      static const char prefix[] = "mosh: process exited with status ";
      char buffer[32];
      size_t len = format_signed_value(code, buffer, sizeof(buffer));
      write_bytes(STDERR_FILENO, prefix, sizeof(prefix) - 1);
      if(len > 0 && len <= sizeof(buffer)) {
        write_bytes(STDERR_FILENO, buffer, len);
      }
      write_char_stdout('\n');
    }
  } else if(WIFSIGNALED(status)) {
    int signo = WTERMSIG(status);
    if(signo != SIGINT) {
      static const char prefix[] = "mosh: process terminated by signal ";
      char buffer[32];
      size_t len = format_signed_value(signo, buffer, sizeof(buffer));
      write_bytes(STDERR_FILENO, prefix, sizeof(prefix) - 1);
      if(len > 0 && len <= sizeof(buffer)) {
        write_bytes(STDERR_FILENO, buffer, len);
      }
      write_char_stdout('\n');
    }
  }
}

static job_event_t job_poll_status(job_t* job, bool block) {
  if(job == NULL) {
    return JOB_EVENT_NONE;
  }

  job_event_t pending_event = JOB_EVENT_NONE;

  while(true) {
    bool progress = false;
    size_t remaining = 0;
    for(size_t i = 0; i < job->segment_count; i++) {
      if(job->pids[i] > 0) {
        remaining++;
      }
    }

    if(remaining == 0) {
      job->state = JOB_STATE_DONE;
      job_handle_exit_messages(job);
      job->last_status = job_raw_status(job);
      return JOB_EVENT_EXITED;
    }

    if(block) {
      if(sigint_requested && shell_take_sigint()) {
        shell_maybe_print_sigint();
        pending_length = 0;
        pending_offset = 0;
        job_send_signal(job, SIGINT);
      }
      if(sigtstp_requested && shell_take_sigtstp()) {
        shell_maybe_print_sigtstp();
        pending_length = 0;
        pending_offset = 0;
        job_send_signal(job, SIGTSTP);
      }
    }

    for(size_t i = 0; i < job->segment_count; i++) {
      long pid = job->pids[i];
      if(pid <= 0) {
        continue;
      }

      int status = 0;
      long waited = syscall3(SYS_WAITPID, pid, (long)&status, WUNTRACED | WCONTINUED | WNOHANG);
      if(waited == pid) {
        progress = true;

        if(WIFSTOPPED(status)) {
          job->statuses[i] = status;
          job->state = JOB_STATE_STOPPED;
          job->foreground = false;
          job->background = false;
          job->last_status = status;
          return JOB_EVENT_STOPPED;
        }

        if(WIFCONTINUED(status)) {
          job->statuses[i] = status;
          job->state = JOB_STATE_RUNNING;
          job->background = true;
          pending_event = JOB_EVENT_CONTINUED;
          continue;
        }

        job->statuses[i] = status;
        job->pids[i] = -pid;
        if(WIFEXITED(status) && WEXITSTATUS(status) == 127 && job->argv0[i][0] != '\0') {
          const char prefix[] = "mosh: command not found: ";
          write_bytes(STDOUT_FILENO, prefix, sizeof(prefix) - 1);
          write_str(STDOUT_FILENO, job->argv0[i]);
          write_char_stdout('\n');
          job->argv0[i][0] = '\0';
        }
      } else if(waited < 0 && waited != -ECHILD) {
        job->pids[i] = -pid;
        job->statuses[i] = (int)waited;
        progress = true;
        write_str(STDOUT_FILENO, "mosh: waitpid failed\n");
      }
    }

    if(pending_event != JOB_EVENT_NONE) {
      job->last_status = job_raw_status(job);
      return pending_event;
    }

    if(!block) {
      return JOB_EVENT_NONE;
    }

    if(!progress) {
      long polled = syscall0(SYS_STDIN_POLL);
      if(polled >= 0) {
        char ch = (char)polled;
        if(ch == 0x03) {
          shell_trigger_sigint();
        } else if(ch == 0x1a) {
          shell_trigger_sigtstp();
        } else {
          if(pending_length + 1 < sizeof(pending_input)) {
            pending_input[pending_length++] = ch;
            pending_input[pending_length] = '\0';
            write_char_stdout(ch);
          }
        }
      } else {
        syscall2(SYS_SLEEP, 1000, 0);
      }
    }
  }
}

static void jobs_poll_updates(bool print_notifications) {
  for(size_t i = 0; i < MOSH_MAX_JOBS; i++) {
    job_t* job = &jobs[i];
    if(!job->in_use) {
      continue;
    }
    if(job->foreground) {
      continue;
    }

    job_event_t event = job_poll_status(job, false);
    if(event == JOB_EVENT_NONE) {
      continue;
    }

    if(event == JOB_EVENT_EXITED) {
      if(print_notifications || job->background) {
        job_print_notification(job, "Done");
      }
      job_release(job);
    } else if(event == JOB_EVENT_STOPPED) {
      job_print_notification(job, "Stopped");
    } else if(event == JOB_EVENT_CONTINUED) {
      if(print_notifications || job->background) {
        job_print_notification(job, "Continued");
      }
    }
  }
}

static int execute_pipeline(const char* command,
                            command_segment_t* segments,
                            size_t segment_count,
                            bool background) {
  long pids[MOSH_MAX_SEGMENTS] = {0};
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
          job_wait_temporary(command, segments, started, pids);
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
        job_wait_temporary(command, segments, started, pids);
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
        int flags = O_WRONLY | O_CREAT;
        if(segments[i].redirect_out_append) {
          flags |= O_APPEND;
        } else {
          flags |= O_TRUNC;
        }
        int fd = open(absolute, flags, 0644);
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

      if(segments[i].redirect_err != NULL) {
        char absolute[MOSH_MAX_PATH];
        if(!normalize_path(current_directory, segments[i].redirect_err, absolute, sizeof(absolute))) {
          write_str(STDOUT_FILENO, "mosh: invalid redirection path\n");
          _exit(1);
        }
        int flags = O_WRONLY | O_CREAT;
        if(segments[i].redirect_err_append) {
          flags |= O_APPEND;
        } else {
          flags |= O_TRUNC;
        }
        int fd = open(absolute, flags, 0644);
        if(fd < 0) {
          write_str(STDOUT_FILENO, "mosh: failed to open redirection target\n");
          _exit(1);
        }
        dup2(fd, STDERR_FILENO);
        if(fd != STDERR_FILENO) {
          close(fd);
        }
      }

      if(segments[i].redirect_err_to_stdout) {
        dup2(STDOUT_FILENO, STDERR_FILENO);
      }

      if(segments[i].redirect_out_to_stderr) {
        dup2(STDERR_FILENO, STDOUT_FILENO);
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
  job_t* job = job_allocate(command, segments, segment_count, pids, background);
  if(job == NULL) {
    write_str(STDOUT_FILENO, "mosh: too many concurrent jobs\n");
    return job_wait_temporary(command, segments, segment_count, pids);
  }

  if(background) {
    job->background = true;
    job->foreground = false;
    char message[64];
    long display_pid = 0;
    if(job->segment_count > 0) {
      display_pid = job->pids[job->segment_count - 1];
      if(display_pid < 0) {
        display_pid = -display_pid;
      }
    }
    size_t msg_len = 0;
    if(msg_len + 1 < sizeof(message)) {
      message[msg_len++] = '[';
    }
    msg_len += format_unsigned_value((size_t)job->id, &message[msg_len], sizeof(message) - msg_len - 1);
    if(msg_len + 2 < sizeof(message)) {
      message[msg_len++] = ']';
      message[msg_len++] = ' ';
    }
    msg_len += format_unsigned_value((size_t)display_pid, &message[msg_len], sizeof(message) - msg_len - 1);
    if(msg_len + 1 < sizeof(message)) {
      message[msg_len++] = '\n';
    }
    message[msg_len] = '\0';
    write_bytes(STDOUT_FILENO, message, msg_len);
    jobs_poll_updates(false);
    return 0;
  }

  int last_status = job_wait_foreground(job);
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
  completion_reset();
  reverse_search_reset();
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

static bool line_replace_range(line_state_t* state, size_t start, size_t end, const char* replacement) {
  if(state == NULL || start > end || end > state->length) {
    return false;
  }

  size_t replacement_len = replacement ? str_len(replacement) : 0;
  size_t tail_len = state->length - end;
  size_t new_len = start + replacement_len + tail_len;
  if(new_len > state->content_capacity) {
    beep();
    return false;
  }

  memmove(&state->buffer[start + replacement_len],
          &state->buffer[end],
          tail_len + 1);

  if(replacement_len > 0) {
    memcpy(&state->buffer[start], replacement, replacement_len);
  }

  state->length = new_len;
  state->buffer[new_len] = '\0';
  state->cursor = start + replacement_len;
  line_redraw(state);
  return true;
}

static void line_insert_char(line_state_t* state, char ch) {
  if(state->length >= state->content_capacity) {
    beep();
    return;
  }
  completion_reset();

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
  completion_reset();

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
  completion_reset();
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
  completion_reset();
  state->cursor--;
  line_redraw(state);
}

static void line_cursor_right(line_state_t* state) {
  if(state->cursor >= state->length) {
    beep();
    return;
  }
  completion_reset();
  state->cursor++;
  line_redraw(state);
}

static void line_cursor_home(line_state_t* state) {
  if(state->cursor == 0) {
    return;
  }
  completion_reset();
  state->cursor = 0;
  line_redraw(state);
}

static void line_cursor_end(line_state_t* state) {
  if(state->cursor == state->length) {
    return;
  }
  completion_reset();
  state->cursor = state->length;
  line_redraw(state);
}

static void line_clear_screen(line_state_t* state) {
  line_hide_caret(state);
  static const char seq[] = "\x1b[2J\x1b[H";
  write_bytes(STDOUT_FILENO, seq, sizeof(seq) - 1);
  state->rendered_length = 0;
  state->needs_carriage_return = false;
  completion_reset();
  line_redraw(state);
}

static bool is_completion_separator(char ch) {
  return ch == ' ' || ch == '\t' || ch == '|' || ch == '>' || ch == '<' || ch == '&' || ch == ';';
}

static bool completion_requires_directory(const line_state_t* state, size_t token_start) {
  if(state == NULL || token_start > state->length) {
    return false;
  }

  size_t start = token_start;
  while(start > 0 && state->buffer[start - 1] == ' ') {
    start--;
  }

  size_t scan = start;
  while(scan > 0) {
    char prev = state->buffer[scan - 1];
    if(prev == '|' || prev == '&' || prev == ';') {
      break;
    }
    scan--;
  }

  size_t cmd_pos = scan;
  while(cmd_pos < state->length && state->buffer[cmd_pos] == ' ') {
    cmd_pos++;
  }
  size_t cmd_end = cmd_pos;
  while(cmd_end < state->length && !is_completion_separator(state->buffer[cmd_end])) {
    cmd_end++;
  }
  size_t cmd_len = cmd_end - cmd_pos;
  if(cmd_len != 2) {
    return false;
  }
  if(str_ncmp(&state->buffer[cmd_pos], "cd", cmd_len) != 0) {
    return false;
  }
  return token_start >= cmd_end;
}

static bool line_attempt_completion(line_state_t* state) {
  if(state == NULL || state->cursor > state->length) {
    return false;
  }

  size_t token_start = state->cursor;
  while(token_start > 0 && !is_completion_separator(state->buffer[token_start - 1])) {
    token_start--;
  }

  size_t token_len = state->cursor - token_start;
  if(token_len >= MOSH_MAX_PATH) {
    return false;
  }

  size_t replace_end = state->cursor;

  if(completion_ctx.active && completion_ctx.match_count > 0 && token_start == completion_ctx.token_start) {
    size_t next_index = (completion_ctx.current_index + 1) % completion_ctx.match_count;

    char new_token[MOSH_MAX_PATH];
    str_copy(new_token, sizeof(new_token), completion_ctx.dir_prefix);
    if(str_len(new_token) + str_len(completion_ctx.matches[next_index]) >= sizeof(new_token)) {
      return false;
    }
    strcat(new_token, completion_ctx.matches[next_index]);

    if(!line_replace_range(state,
                           completion_ctx.token_start,
                           completion_ctx.token_start + completion_ctx.inserted_length,
                           new_token)) {
      return false;
    }

    completion_ctx.inserted_length = str_len(new_token);
    completion_ctx.current_index = next_index;
    line_redraw(state);
    return true;
  }

  completion_reset();

  char token[MOSH_MAX_PATH];
  for(size_t i = 0; i < token_len; i++) {
    token[i] = state->buffer[token_start + i];
  }
  token[token_len] = '\0';

  size_t last_slash = 0;
  bool has_slash = false;
  for(size_t i = 0; i < token_len; i++) {
    if(token[i] == '/') {
      has_slash = true;
      last_slash = i;
    }
  }

  char dir_prefix[MOSH_MAX_PATH];
  if(has_slash) {
    size_t prefix_len = last_slash + 1;
    if(prefix_len >= sizeof(dir_prefix)) {
      return false;
    }
    memcpy(dir_prefix, &state->buffer[token_start], prefix_len);
    dir_prefix[prefix_len] = '\0';
  } else {
    dir_prefix[0] = '\0';
  }

  const char* prefix = has_slash ? token + last_slash + 1 : token;
  size_t prefix_len = str_len(prefix);

  char search_dir[MOSH_MAX_PATH];
  if(has_slash) {
    if(!normalize_path(current_directory, dir_prefix, search_dir, sizeof(search_dir))) {
      return false;
    }
  } else {
    str_copy(search_dir, sizeof(search_dir), current_directory);
  }

  char listing[4096];
  long rc = syscall3(SYS_LISTDIR, (long)search_dir, (long)listing, (long)(sizeof(listing) - 1));
  if(rc < 0) {
    return false;
  }

  size_t list_len = (size_t)rc;
  if(list_len >= sizeof(listing)) {
    list_len = sizeof(listing) - 1;
  }
  listing[list_len] = '\0';

  char (*matches)[MOSH_MAX_PATH] = completion_ctx.matches;
  bool* match_is_dir = completion_ctx.match_is_dir;
  size_t match_count = 0;
  bool require_directory = completion_requires_directory(state, token_start);

  size_t pos = 0;
  while(pos < list_len) {
    char* entry = &listing[pos];
    size_t len = 0;
    while((pos + len) < list_len && listing[pos + len] != '\n') {
      len++;
    }
    listing[pos + len] = '\0';
    pos += len + 1;

    if(len == 0 || prefix_len > len) {
      continue;
    }
    if(str_ncmp(entry, prefix, prefix_len) != 0) {
      continue;
    }
    if(match_count < MOSH_MAX_COMPLETIONS) {
      bool is_dir = (len > 0 && entry[len - 1] == '/');
      if(require_directory && !is_dir) {
        continue;
      }
      if(is_dir) {
        len--;
      }
      if(len >= sizeof(matches[match_count])) {
        len = sizeof(matches[match_count]) - 1;
      }
      memcpy(matches[match_count], entry, len);
      matches[match_count][len] = '\0';
      match_is_dir[match_count] = is_dir;
      match_count++;
    }
  }

  if(match_count == 0) {
    return false;
  }

  for(size_t i = 0; i < match_count; i++) {
    for(size_t j = i + 1; j < match_count; j++) {
      if(strcmp(matches[i], matches[j]) > 0) {
        char tmp[MOSH_MAX_PATH];
        str_copy(tmp, sizeof(tmp), matches[i]);
        str_copy(matches[i], sizeof(matches[i]), matches[j]);
        str_copy(matches[j], sizeof(matches[j]), tmp);
        bool dir_tmp = match_is_dir[i];
        match_is_dir[i] = match_is_dir[j];
        match_is_dir[j] = dir_tmp;
      }
    }
  }

  char effective_prefix[MOSH_MAX_PATH];
  if(dir_prefix[0] != '\0') {
    str_copy(effective_prefix, sizeof(effective_prefix), dir_prefix);
  } else if(search_dir[0] == '/' && search_dir[1] == '\0') {
    str_copy(effective_prefix, sizeof(effective_prefix), "/");
  } else {
    effective_prefix[0] = '\0';
  }

  completion_ctx.token_start = token_start;
  completion_ctx.match_count = match_count;
  completion_ctx.current_index = match_count; // ensures next cycle starts at first entry
  str_copy(completion_ctx.dir_prefix, sizeof(completion_ctx.dir_prefix), effective_prefix);

  char new_token[MOSH_MAX_PATH];

  if(match_count == 1) {
    str_copy(new_token, sizeof(new_token), effective_prefix);
    if(str_len(new_token) + str_len(matches[0]) >= sizeof(new_token)) {
      return false;
    }
    strcat(new_token, matches[0]);
    if(!line_replace_range(state, token_start, replace_end, new_token)) {
      return false;
    }
    completion_ctx.inserted_length = str_len(new_token);
    completion_ctx.current_index = 0;
    completion_ctx.active = false;
    line_redraw(state);
    return true;
  }

  char lcp[MOSH_MAX_PATH];
  str_copy(lcp, sizeof(lcp), matches[0]);
  for(size_t i = 1; i < match_count && lcp[0] != '\0'; i++) {
    size_t j = 0;
    while(lcp[j] != '\0' && matches[i][j] != '\0' && lcp[j] == matches[i][j]) {
      j++;
    }
    lcp[j] = '\0';
  }

  size_t lcp_len = str_len(lcp);
  str_copy(new_token, sizeof(new_token), effective_prefix);

  if(lcp_len > prefix_len) {
    if(str_len(new_token) + lcp_len >= sizeof(new_token)) {
      return false;
    }
    strcat(new_token, lcp);
    if(!line_replace_range(state, token_start, replace_end, new_token)) {
      return false;
    }
    completion_ctx.inserted_length = str_len(new_token);
    completion_ctx.active = true;
    completion_ctx.current_index = match_count; // next tab starts at first entry
    line_redraw(state);
    return true;
  }

  str_copy(new_token, sizeof(new_token), effective_prefix);
  if(str_len(new_token) + str_len(matches[0]) >= sizeof(new_token)) {
    return false;
  }
  strcat(new_token, matches[0]);
  if(!line_replace_range(state, token_start, replace_end, new_token)) {
    return false;
  }
  completion_ctx.inserted_length = str_len(new_token);
  completion_ctx.current_index = 0;
  completion_ctx.active = true;
  line_redraw(state);
  return true;
}

static int read_stdin_char(void) {
  char ch = 0;
  long rc = read(STDIN_FILENO, &ch, 1);
#ifdef MENIOS_HOST_TEST
  if(rc <= 0) {
    return -1;
  }
  return (unsigned char)ch;
#else
  if(rc <= 0) {
    fprintf(stderr, "mosh: read rc=%ld errno=%d\n", rc, errno);
    return -1;
  }
  fprintf(stderr, "mosh: read char=0x%02x (%c)\n", (unsigned char)ch,
          (ch >= 32 && ch < 127) ? ch : '.');
  return (unsigned char)ch;
#endif
}

static char history_entries[MOSH_HISTORY_LIMIT][MOSH_MAX_LINE_LENGTH];
static size_t history_length = 0;
static size_t history_next = 0;
static size_t history_cursor = 0;

static void history_reset(void) {
  for(size_t i = 0; i < MOSH_HISTORY_LIMIT; i++) {
    history_entries[i][0] = '\0';
  }
  history_length = 0;
  history_next = 0;
  history_cursor = 0;
  completion_reset();
  reverse_search_reset();
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
  completion_reset();
  line_redraw(state);
}

static void reverse_search_clear_display(line_state_t* state) {
  line_hide_caret(state);
  write_char_stdout('\r');
  static const char clear_seq[] = "\x1b[K";
  write_bytes(STDOUT_FILENO, clear_seq, sizeof(clear_seq) - 1);
  state->caret_visible = false;
  state->needs_carriage_return = true;
}

static void reverse_search_reset(void) {
  reverse_search.active = false;
  reverse_search.have_match = false;
  reverse_search.saved_valid = false;
  reverse_search.query_len = 0;
  reverse_search.query[0] = '\0';
  reverse_search.search_index = history_length;
  reverse_search.match_index = history_length;
}

static bool reverse_search_scan(size_t start, const char** out_entry, size_t* out_index) {
  size_t idx = start;
  while(idx > 0) {
    idx--;
    const char* entry = history_get(idx);
    if(reverse_search.query_len == 0 || str_find_substring((char*)entry, reverse_search.query) != NULL) {
      if(out_index != NULL) {
        *out_index = idx;
      }
      if(out_entry != NULL) {
        *out_entry = entry;
      }
      return true;
    }
  }
  return false;
}

static void reverse_search_render(line_state_t* state) {
  reverse_search_clear_display(state);

  char message[MOSH_MAX_LINE_LENGTH * 2];
  size_t offset = 0;
  offset = str_append(message, sizeof(message), offset, "(reverse-search: '");
  offset = str_append(message, sizeof(message), offset, reverse_search.query);
  offset = str_append(message, sizeof(message), offset, "') ");
  if(reverse_search.have_match) {
    offset = str_append(message, sizeof(message), offset, state->buffer);
  } else {
    offset = str_append(message, sizeof(message), offset, "- no match");
  }
  write_bytes(STDOUT_FILENO, message, offset);
}

static void reverse_search_copy_to_state(line_state_t* state, const char* entry) {
  str_copy(state->buffer, state->total_capacity, entry);
  state->length = str_len(state->buffer);
  if(state->length > state->content_capacity) {
    state->length = state->content_capacity;
    state->buffer[state->length] = '\0';
  }
  state->cursor = state->length;
  state->rendered_length = state->length;
  state->needs_carriage_return = true;
  completion_reset();
}

static void reverse_search_apply(line_state_t* state, bool advance_from_current) {
  size_t start = reverse_search.search_index;
  if(advance_from_current && reverse_search.have_match) {
    start = reverse_search.match_index;
  }

  size_t match_idx = 0;
  const char* match_entry = NULL;
  bool found = reverse_search_scan(start, &match_entry, &match_idx);
  if(!found && reverse_search.have_match) {
    found = reverse_search_scan(history_length, &match_entry, &match_idx);
  } else if(!found && reverse_search.query_len == 0) {
    found = reverse_search_scan(history_length, &match_entry, &match_idx);
  }

  if(found) {
    reverse_search.have_match = true;
    reverse_search.match_index = match_idx;
    reverse_search.search_index = match_idx;
    reverse_search_copy_to_state(state, match_entry);
    reverse_search_render(state);
    return;
  }

  reverse_search.have_match = false;
  reverse_search.search_index = history_length;
  reverse_search_render(state);
  beep();
}

static void reverse_search_start(line_state_t* state) {
  if(reverse_search.active) {
    reverse_search_apply(state, true);
    return;
  }

  reverse_search.active = true;
  reverse_search.have_match = false;
  reverse_search.query_len = 0;
  reverse_search.query[0] = '\0';
  reverse_search.search_index = history_length;
  reverse_search.match_index = history_length;
  str_copy(reverse_search.saved_buffer, sizeof(reverse_search.saved_buffer), state->buffer);
  reverse_search.saved_length = str_len(reverse_search.saved_buffer);
  reverse_search.saved_cursor = state->cursor;
  reverse_search.saved_valid = true;
  history_cursor = history_length;

  reverse_search_apply(state, false);
}

static void reverse_search_next(line_state_t* state) {
  if(!reverse_search.active) {
    reverse_search_start(state);
    return;
  }
  if(reverse_search.have_match && reverse_search.match_index > 0) {
    reverse_search.search_index = reverse_search.match_index;
  }
  reverse_search_apply(state, true);
}

static void reverse_search_append_char(line_state_t* state, char ch) {
  if(reverse_search.query_len + 1 >= sizeof(reverse_search.query)) {
    beep();
    return;
  }
  reverse_search.query[reverse_search.query_len++] = ch;
  reverse_search.query[reverse_search.query_len] = '\0';
  reverse_search.have_match = false;
  reverse_search.search_index = history_length;
  reverse_search_apply(state, false);
}

static void reverse_search_backspace(line_state_t* state) {
  if(reverse_search.query_len == 0) {
    beep();
    return;
  }
  reverse_search.query_len--;
  reverse_search.query[reverse_search.query_len] = '\0';
  reverse_search.have_match = false;
  reverse_search.search_index = history_length;
  reverse_search_apply(state, false);
}

static void reverse_search_accept(line_state_t* state) {
  if(!reverse_search.have_match && reverse_search.saved_valid) {
    str_copy(state->buffer, state->total_capacity, reverse_search.saved_buffer);
    state->length = str_len(state->buffer);
    if(state->length > state->content_capacity) {
      state->length = state->content_capacity;
      state->buffer[state->length] = '\0';
    }
    state->cursor = (reverse_search.saved_cursor <= state->length) ? reverse_search.saved_cursor : state->length;
    state->rendered_length = state->length;
    state->needs_carriage_return = true;
    completion_reset();
  }
  reverse_search_clear_display(state);
  reverse_search_reset();
  line_redraw(state);
}

static void reverse_search_restore_saved(line_state_t* state) {
  if(!reverse_search.saved_valid) {
    state->length = 0;
    state->buffer[0] = '\0';
    state->cursor = 0;
    state->rendered_length = 0;
    return;
  }

  str_copy(state->buffer, state->total_capacity, reverse_search.saved_buffer);
  state->length = str_len(state->buffer);
  if(state->length > state->content_capacity) {
    state->length = state->content_capacity;
    state->buffer[state->length] = '\0';
  }
  state->cursor = (reverse_search.saved_cursor <= state->length) ? reverse_search.saved_cursor : state->length;
  state->rendered_length = state->length;
  state->needs_carriage_return = true;
  completion_reset();
}

static void reverse_search_cancel(line_state_t* state) {
  reverse_search_restore_saved(state);
  reverse_search_clear_display(state);
  reverse_search_reset();
  line_redraw(state);
  beep();
}

static bool reverse_search_handle_char(line_state_t* state, char ch) {
  if(ch == 0x12) {
    reverse_search_next(state);
    return true;
  }
  if(ch == '\b' || ch == 0x7f) {
    reverse_search_backspace(state);
    return true;
  }
  if(ch == 0x07) {
    reverse_search_cancel(state);
    return true;
  }
  if(ch == '\n' || ch == '\r') {
    reverse_search_accept(state);
    return false;
  }
  if(ch == 0x1b) {
    reverse_search_accept(state);
    return false;
  }
  if(ch >= 0x20 && ch < 0x7f) {
    reverse_search_append_char(state, ch);
    return true;
  }

  reverse_search_accept(state);
  return false;
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

  if(prompt != NULL) {
    history_cursor = history_length;
  }

  enum escape_state esc_state = ESCAPE_NONE;
  char esc_params[8];
  size_t esc_param_len = 0;
  bool swallow_lf = false;

  while(true) {
    if(sigint_requested && shell_take_sigint()) {
      line_hide_caret(&state);
      shell_maybe_print_sigint();
      state.length = 0;
      state.cursor = 0;
      state.buffer[0] = '\0';
      state.rendered_length = 0;
      state.needs_carriage_return = false;
      scratch_active = false;
      reverse_search_reset();
      pending_length = 0;
      pending_offset = 0;
      history_cursor = history_length;
      return 0;
    }

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
        if(ch == 0x03) {
          shell_trigger_sigint();
          continue;
        }
        if(reverse_search.active) {
          if(reverse_search_handle_char(&state, ch)) {
            break;
          }
        }
        if(ch == '\n' || ch == '\r') {
          state.buffer[state.length] = '\0';
          line_hide_caret(&state);
          write_char_stdout('\n');
          if(ch == '\r') {
            swallow_lf = true;
          }
          history_cursor = history_length;
          return state.length;
        }
        if(ch == 0x12) {
          if(history_length == 0) {
            beep();
          } else {
            reverse_search_start(&state);
          }
          break;
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
          if(!line_attempt_completion(&state)) {
            beep();
          }
          break;
        }
        if(ch == 0x01) {
          line_cursor_home(&state);
          break;
        }
        if(ch == 0x05) {
          line_cursor_end(&state);
          break;
        }
        if(ch == 0x0c) {
          line_clear_screen(&state);
          break;
        }
        if(ch >= 0x20 && ch < 0x7f) {
          line_insert_char(&state, ch);
          break;
        }
        if(ch == 0x04) {
          if(state.length == 0) {
            line_hide_caret(&state);
            history_cursor = history_length;
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

static bool handle_builtin(const char* raw_line, char* line, int* out_status) {
  if(raw_line == NULL || line == NULL) {
    return false;
  }

  if(shell_function_define_inline(raw_line, out_status)) {
    return true;
  }

  size_t idx = 0;
  while(line[idx] == ' ' || line[idx] == '\t') {
    idx++;
  }

  const char* command_start = line + idx;
  while(line[idx] != '\0' && line[idx] != ' ' && line[idx] != '\t') {
    idx++;
  }
  size_t command_len = (size_t)(line + idx - command_start);

  while(line[idx] == ' ' || line[idx] == '\t') {
    idx++;
  }
  char* arguments = line + idx;

  const char* raw_cursor = raw_line;
  while(*raw_cursor == ' ' || *raw_cursor == '\t') {
    raw_cursor++;
  }
  const char* raw_command_start = raw_cursor;
  while(*raw_cursor != '\0' && *raw_cursor != ' ' && *raw_cursor != '\t') {
    raw_cursor++;
  }
  size_t raw_command_len = (size_t)(raw_cursor - raw_command_start);
  while(*raw_cursor == ' ' || *raw_cursor == '\t') {
    raw_cursor++;
  }
  const char* raw_arguments = raw_cursor;

  if(command_len == 0) {
    shell_finish_builtin_code(0, out_status);
    return true;
  }

  if(command_len == 8 && str_ncmp(command_start, "function", 8) == 0) {
    if(!shell_function_define_keyword(raw_arguments, arguments, out_status)) {
      shell_finish_builtin_code(1, out_status);
    }
    return true;
  }

#ifdef MOSH_TEST
  if(command_len == 14 && str_ncmp(command_start, "__test_success", 14) == 0) {
    shell_finish_builtin_code(0, out_status);
    return true;
  }
  if(command_len == 14 && str_ncmp(command_start, "__test_failure", 14) == 0) {
    shell_finish_builtin_code(1, out_status);
    return true;
  }
  if(command_len == 18 && str_ncmp(command_start, "__test_set_counter", 18) == 0) {
    if(!builtin_set_test_counter(arguments, out_status)) {
      shell_finish_builtin_code(1, out_status);
    }
    return true;
  }
  if(command_len == 16 && str_ncmp(command_start, "__test_counter_lt", 16) == 0) {
    if(!builtin_test_counter_lt(out_status)) {
      shell_finish_builtin_code(1, out_status);
    }
    return true;
  }
#endif

  if(command_len == 4 && str_ncmp(command_start, "help", 4) == 0) {
    write_str(STDOUT_FILENO,
              "Built-ins:\n"
              "  help  - show this message\n"
              "  exit  - leave mosh\n"
              "  pwd   - print current directory\n"
              "  echo  - print arguments\n"
              "  time  - measure command duration\n"
              "  cd    - change directory (limited)\n"
              "  jobs  - list background jobs\n"
              "  fg    - resume job in foreground\n"
              "  bg    - resume job in background\n"
              "  set   - assign shell variable\n"
              "  unset - remove shell variable\n"
              "  if/while/for/function - scripting constructs\n");
    shell_finish_builtin_code(0, out_status);
    return true;
  }

  if(command_len == 4 && str_ncmp(command_start, "time", 4) == 0) {
    if(raw_arguments[0] == '\0') {
      write_str(STDERR_FILENO, "mosh: time: missing command\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }

    struct timespec start_ts;
    bool have_start = (clock_gettime(CLOCK_MONOTONIC, &start_ts) == 0);

    char timed_command[MOSH_MAX_FUNCTION_BODY];
    str_copy(timed_command, sizeof(timed_command), raw_arguments);

    int raw_result = launch_command(timed_command);

    bool printed = false;
    if(have_start) {
      struct timespec end_ts;
      if(clock_gettime(CLOCK_MONOTONIC, &end_ts) == 0) {
        long long seconds = 0;
        long long nanoseconds = 0;
        if(compute_timespec_diff(&start_ts, &end_ts, &seconds, &nanoseconds)) {
          print_duration_line("real", seconds, nanoseconds);
          write_str(STDERR_FILENO, "user n/a\n");
          write_str(STDERR_FILENO, "sys  n/a\n");
          printed = true;
        }
      }
    }

    if(!printed) {
      write_str(STDERR_FILENO, "mosh: time: unable to measure duration\n");
    }

    shell_finish_builtin_raw(raw_result, out_status);
    return true;
  }

  if(command_len == 4 && str_ncmp(command_start, "exit", 4) == 0) {
    write_str(STDOUT_FILENO, "bye\n");
    _exit(0);
    return true;
  }

  if(command_len == 3 && str_ncmp(command_start, "pwd", 3) == 0) {
    write_str(STDOUT_FILENO, current_directory);
    write_str(STDOUT_FILENO, "\n");
    shell_finish_builtin_code(0, out_status);
    return true;
  }

  if(command_len == 3 && str_ncmp(command_start, "set", 3) == 0) {
    return builtin_set_variable(arguments, out_status);
  }

  if(command_len == 6 && str_ncmp(command_start, "export", 6) == 0) {
    return builtin_export_variable(arguments, out_status);
  }

  if(command_len == 5 && str_ncmp(command_start, "unset", 5) == 0) {
    return builtin_unset_variable(arguments, out_status);
  }

  if(command_len == 2 && str_ncmp(command_start, "if", 2) == 0) {
    return builtin_if(raw_arguments, arguments, out_status);
  }

  if(command_len == 5 && str_ncmp(command_start, "while", 5) == 0) {
    return builtin_while((char*)raw_arguments, out_status);
  }

  if(command_len == 3 && str_ncmp(command_start, "for", 3) == 0) {
    return builtin_for((char*)raw_arguments, out_status);
  }

  if(command_len == 4 && str_ncmp(command_start, "echo", 4) == 0) {
    bool has_redirection = false;
    if(raw_line != NULL) {
      bool in_quotes = false;
      for(size_t i = 0; raw_line[i] != '\0'; i++) {
        char ch = raw_line[i];
        if(ch == '"') {
          in_quotes = !in_quotes;
          continue;
        }
        if(!in_quotes && (ch == '>' || ch == '<' || ch == '|')) {
          has_redirection = true;
          break;
        }
      }
    }

    if(has_redirection) {
      return false;
    }
    size_t arg_len = str_len(arguments);
    bool quoted = (arg_len >= 2 && arguments[0] == '"' && arguments[arg_len - 1] == '"');
    if(quoted) {
      write_bytes(STDOUT_FILENO, arguments + 1, arg_len - 2);
    } else {
      write_str(STDOUT_FILENO, arguments);
    }
    write_str(STDOUT_FILENO, "\n");
    shell_finish_builtin_code(0, out_status);
    return true;
  }

  if(command_len == 4 && str_ncmp(command_start, "jobs", 4) == 0) {
    jobs_poll_updates(true);
    jobs_print_list();
    shell_finish_builtin_code(0, out_status);
    return true;
  }

  if(command_len == 2 && str_ncmp(command_start, "fg", 2) == 0) {
    jobs_poll_updates(false);
    job_t* job = job_resolve_argument(arguments, true);
    if(job == NULL) {
      write_str(STDOUT_FILENO, "mosh: fg: job not found\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }
    if(job->state == JOB_STATE_DONE) {
      write_str(STDOUT_FILENO, "mosh: fg: job already completed\n");
      job_release(job);
      shell_finish_builtin_code(1, out_status);
      return true;
    }
    write_str(STDOUT_FILENO, job->command);
    write_char_stdout('\n');
    job_send_signal(job, SIGCONT);
    int status = job_wait_foreground(job);
    shell_finish_builtin_raw(status, out_status);
    return true;
  }

  if(command_len == 2 && str_ncmp(command_start, "bg", 2) == 0) {
    jobs_poll_updates(false);
    job_t* job = job_resolve_argument(arguments, false);
    if(job == NULL) {
      write_str(STDOUT_FILENO, "mosh: bg: job not found\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }
    if(job->state == JOB_STATE_DONE) {
      write_str(STDOUT_FILENO, "mosh: bg: job already completed\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }
    if(job->state != JOB_STATE_STOPPED) {
      write_str(STDOUT_FILENO, "mosh: bg: job not stopped\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }
    job->background = true;
    job->foreground = false;
    job_send_signal(job, SIGCONT);
    job_print_notification(job, "Continued");
    jobs_poll_updates(false);
    shell_finish_builtin_code(0, out_status);
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
        shell_finish_builtin_code(1, out_status);
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
        shell_finish_builtin_code(1, out_status);
        return true;
      }
      for(size_t i = 0; i < consumed; i++) {
        temp[i] = extra[i];
      }
      temp[consumed] = '\0';
      if(!normalize_path(current_directory, temp, resolved, sizeof(resolved))) {
        write_str(STDOUT_FILENO, "mosh: cd: invalid path\n");
        shell_finish_builtin_code(1, out_status);
        return true;
      }
      while(target[consumed] == ' ' || target[consumed] == '\t') {
        consumed++;
      }
      if(target[consumed] != '\0') {
        write_str(STDOUT_FILENO, "mosh: cd: too many arguments\n");
        shell_finish_builtin_code(1, out_status);
        return true;
      }
    }

    if(chdir(resolved) != 0) {
      write_str(STDOUT_FILENO, "mosh: cd: unable to access directory\n");
      shell_finish_builtin_code(1, out_status);
      return true;
    }

    str_copy(current_directory, sizeof(current_directory), resolved);
    env_set("PWD", current_directory);
    shell_finish_builtin_code(0, out_status);
    return true;
  }

  shell_function_t* fn = shell_function_lookup(command_start);
  if(fn != NULL) {
    char arg_copy[MOSH_MAX_FUNCTION_BODY];
    str_copy(arg_copy, sizeof(arg_copy), arguments);
    if(shell_function_call(fn, arg_copy, out_status)) {
      return true;
    }
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

static size_t format_hex_uintptr(uintptr_t value, char* out, size_t capacity) {
  if(out == NULL || capacity == 0) {
    return 0;
  }
  if(capacity < 3) {
    out[0] = '\0';
    return 0;
  }

  static const char hex_digits[] = "0123456789abcdef";
  size_t len = 0;
  out[len++] = '0';
  if(len < capacity) {
    out[len++] = 'x';
  }

  bool started = false;
  for(int shift = (int)(sizeof(uintptr_t) * 8 - 4); shift >= 0; shift -= 4) {
    uint8_t nibble = (uint8_t)((value >> shift) & 0xfu);
    if(!started && nibble == 0 && shift != 0) {
      continue;
    }
    started = true;
    if(len + 1 >= capacity) {
      break;
    }
    out[len++] = hex_digits[nibble];
  }

  if(!started) {
    if(len + 1 < capacity) {
      out[len++] = '0';
    }
  }

  if(len < capacity) {
    out[len] = '\0';
  } else {
    out[capacity - 1] = '\0';
    len = capacity - 1;
  }
  return len;
}

static size_t format_signed_long(long value, char* out, size_t capacity) {
  if(out == NULL || capacity == 0) {
    return 0;
  }

  size_t index = 0;
  unsigned long magnitude;
  if(value < 0) {
    if(index + 1 >= capacity) {
      out[0] = '\0';
      return 0;
    }
    out[index++] = '-';
    magnitude = (unsigned long)(-value);
  } else {
    magnitude = (unsigned long)value;
  }

  size_t written = format_unsigned_value((size_t)magnitude, &out[index], capacity - index);
  return index + written;
}

static void debug_write_ptr(int fd, const void* ptr) {
  char buf[2 + sizeof(uintptr_t) * 2 + 1];
  format_hex_uintptr((uintptr_t)ptr, buf, sizeof(buf));
  write_str(fd, buf);
}

static void debug_write_long_field(int fd, const char* label, long value) {
  if(label != NULL) {
    write_str(fd, label);
  }
  char buf[32];
  format_signed_long(value, buf, sizeof(buf));
  write_str(fd, buf);
}

static void debug_write_int_field(int fd, const char* label, int value) {
  if(label != NULL) {
    write_str(fd, label);
  }
  char buf[16];
  format_signed_value(value, buf, sizeof(buf));
  write_str(fd, buf);
}

static void debug_log_exec_attempt(const char* stage,
                                   const char* path,
                                   char* const* argv,
                                   char* const* envp) {
  write_str(STDERR_FILENO, "[mosh] exec attempt stage=");
  write_str(STDERR_FILENO, (stage != NULL) ? stage : "?");
  write_str(STDERR_FILENO, " path_ptr=");
  debug_write_ptr(STDERR_FILENO, path);
  write_str(STDERR_FILENO, " argv=");
  debug_write_ptr(STDERR_FILENO, argv);
  write_str(STDERR_FILENO, " argv0=");
  debug_write_ptr(STDERR_FILENO, (argv != NULL) ? argv[0] : NULL);
  write_str(STDERR_FILENO, " envp=");
  debug_write_ptr(STDERR_FILENO, envp);
  write_str(STDERR_FILENO, " env0=");
  debug_write_ptr(STDERR_FILENO, (envp != NULL && envp[0] != NULL) ? envp[0] : NULL);
  write_str(STDERR_FILENO, "\n");
}

static void debug_log_exec_result(const char* stage,
                                  const char* path,
                                  long rc) {
  write_str(STDERR_FILENO, "[mosh] exec result stage=");
  write_str(STDERR_FILENO, (stage != NULL) ? stage : "?");
  write_str(STDERR_FILENO, " path_ptr=");
  debug_write_ptr(STDERR_FILENO, path);
  write_str(STDERR_FILENO, " rc=");
  debug_write_long_field(STDERR_FILENO, NULL, rc);
  write_str(STDERR_FILENO, " errno=");
  debug_write_int_field(STDERR_FILENO, NULL, errno);
  write_str(STDERR_FILENO, "\n");
}

static void debug_log_expand_start(const char* input, char* output, size_t capacity) {
  write_str(STDERR_FILENO, "[mosh] expand start input=");
  debug_write_ptr(STDERR_FILENO, input);
  write_str(STDERR_FILENO, " output=");
  debug_write_ptr(STDERR_FILENO, output);
  write_str(STDERR_FILENO, " cap=");
  char buf[32];
  format_unsigned_value(capacity, buf, sizeof(buf));
  write_str(STDERR_FILENO, buf);
  write_str(STDERR_FILENO, " input_text=");
  if(input != NULL) {
    if(debug_ptr_readable(input)) {
      write_bytes(STDERR_FILENO, input, str_len(input));
    } else {
      write_str(STDERR_FILENO, "<invalid>");
    }
  } else {
    write_str(STDERR_FILENO, "(null)");
  }
  write_str(STDERR_FILENO, "\n");
  debug_dump_bytes("[mosh] expand input", input);
}

static void debug_log_expand_dollar(const char* cursor) {
  write_str(STDERR_FILENO, "[mosh] expand dollar cursor=");
  debug_write_ptr(STDERR_FILENO, cursor);
  write_str(STDERR_FILENO, " values=");
  if(cursor != NULL) {
    if(debug_ptr_readable(cursor)) {
      write_bytes(STDERR_FILENO, cursor, str_len(cursor));
    } else {
      write_str(STDERR_FILENO, "<invalid>");
    }
  }
  write_str(STDERR_FILENO, "\n");
  debug_dump_bytes("[mosh] expand dollar bytes", cursor);
}

static void debug_dump_bytes(const char* label, const char* data) {
  write_str(STDERR_FILENO, label);
  if(data == NULL) {
    write_str(STDERR_FILENO, "(null)\n");
    return;
  }
  write_str(STDERR_FILENO, " bytes=");
  if(debug_ptr_readable(data)) {
    const unsigned char* bytes = (const unsigned char*)data;
    for(size_t idx = 0; idx < 64 && bytes[idx] != '\0'; idx++) {
      char buf[4];
      buf[0] = "0123456789abcdef"[(bytes[idx] >> 4) & 0xf];
      buf[1] = "0123456789abcdef"[bytes[idx] & 0xf];
      buf[2] = ' ';
      buf[3] = '\0';
      write_str(STDERR_FILENO, buf);
    }
  } else {
    write_str(STDERR_FILENO, "<invalid>");
  }
  write_str(STDERR_FILENO, "\n");
}

static void print_exec_error(const char* command, int err, bool not_found) {
  if(command == NULL) {
    command = "";
  }

  if(not_found) {
    write_str(STDERR_FILENO, "mosh: command not found: ");
    write_str(STDERR_FILENO, command);
    write_str(STDERR_FILENO, "\n");
    return;
  }

  write_str(STDERR_FILENO, "mosh: exec failed");
  if(command[0] != '\0') {
    write_str(STDERR_FILENO, ": ");
    write_str(STDERR_FILENO, command);
  }

  if(err != 0) {
    write_str(STDERR_FILENO, ": ");
    const char* err_text = strerror(err);
    if(err_text != NULL && err_text[0] != '\0') {
      write_str(STDERR_FILENO, err_text);
    } else {
      write_str(STDERR_FILENO, "error ");
      char buf[32];
      format_signed_value(err, buf, sizeof(buf));
      write_str(STDERR_FILENO, buf);
    }
  }

  write_str(STDERR_FILENO, "\n");
}

static bool debug_ptr_readable(const void* ptr) {
  uintptr_t value = (uintptr_t)ptr;
  const uintptr_t MIN_USER_PTR = 0x1000u;
  const uintptr_t MAX_USER_PTR = 0x00007fffffffffffULL;
  return value >= MIN_USER_PTR && value <= MAX_USER_PTR;
}

static void exec_command(char** argv, size_t argc) {
  if(argv == NULL || argc == 0 || argv[0] == NULL) {
    _exit(0);
    return;
  }

  char** envp = (process_envp != NULL) ? process_envp : fallback_envp;
  const char* command = argv[0];

  if(contains_slash(command)) {
    debug_log_exec_attempt("direct", command, argv, envp);
    long rc = mosh_execve(command, argv, envp);
    int err = errno;
    debug_log_exec_result("direct", command, rc);
    if(rc < 0) {
      print_exec_error(command, err, false);
      errno = err;
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
  int last_err = 0;
  bool have_error = false;
  bool saw_non_enent = false;

  while(true) {
    size_t segment_len = 0;
    while(segment[segment_len] != '\0' && segment[segment_len] != ':') {
      segment_len++;
    }
    bool at_end = (segment[segment_len] == '\0');

    if(segment_len == 0) {
      debug_log_exec_attempt("empty_path", command, argv, envp);
      long rc = mosh_execve(command, argv, envp);
      int err = errno;
      debug_log_exec_result("empty_path", command, rc);
      if(rc >= 0) {
        return;
      }
      last_err = err;
      have_error = true;
      if(err != ENOENT) {
        saw_non_enent = true;
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
        debug_log_exec_attempt("path", candidate, argv, envp);
        long rc = mosh_execve(candidate, argv, envp);
        int err = errno;
        debug_log_exec_result("path", candidate, rc);
        if(rc >= 0) {
          return;
        }
        last_err = err;
        have_error = true;
        if(err != ENOENT) {
          saw_non_enent = true;
        }
      }
    }

    if(at_end) {
      break;
    }
    segment += segment_len + 1;
  }

  bool not_found = !saw_non_enent;
  if(!have_error) {
    not_found = true;
    last_err = ENOENT;
  }
  print_exec_error(command, last_err, not_found);
  errno = last_err;
  _exit(not_found ? 127 : 126);
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

static void shell_run_startup_script(void) {
  char home_resolved[MOSH_MAX_PATH];
  const char* home = env_get("HOME");
  bool have_home = false;
  if(home != NULL && home[0] != '\0') {
    have_home = normalize_path("/", home, home_resolved, sizeof(home_resolved));
  }
  if(!have_home || str_eq(home_resolved, "/")) {
    str_copy(home_resolved, sizeof(home_resolved), "/home");
  }

  char script_path[MOSH_MAX_PATH];
  if(!normalize_path(home_resolved, ".moshrc", script_path, sizeof(script_path))) {
    return;
  }

  int fd = open(script_path, O_RDONLY);
  if(fd < 0) {
    return;
  }

  char read_buf[128];
  char line[MOSH_MAX_FUNCTION_BODY];
  size_t line_len = 0;
  bool discard = false;

  while(true) {
    ssize_t rc = read(fd, read_buf, sizeof(read_buf));
    if(rc <= 0) {
      break;
    }
    for(ssize_t i = 0; i < rc; i++) {
      char ch = read_buf[i];
      if(ch == '\r') {
        continue;
      }
      if(ch == '\n') {
        if(!discard && line_len < sizeof(line)) {
          line[line_len] = '\0';
          rtrim(line);
          char* trimmed = ltrim(line);
          if(trimmed[0] != '\0' && trimmed[0] != '#') {
            char command_buf[MOSH_MAX_FUNCTION_BODY];
            str_copy(command_buf, sizeof(command_buf), trimmed);
            launch_command(command_buf);
          }
        }
        line_len = 0;
        discard = false;
        continue;
      }

      if(discard) {
        continue;
      }

      if(line_len + 1 < sizeof(line)) {
        line[line_len++] = ch;
      } else {
        discard = true;
      }
    }
  }

  if(!discard && line_len > 0) {
    line[line_len] = '\0';
    rtrim(line);
    char* trimmed = ltrim(line);
    if(trimmed[0] != '\0' && trimmed[0] != '#') {
      char command_buf[MOSH_MAX_FUNCTION_BODY];
      str_copy(command_buf, sizeof(command_buf), trimmed);
      launch_command(command_buf);
    }
  }

  close(fd);
}

static size_t split_sequence(char* line, char* parts[], sequence_op_t ops[], size_t max_parts) {
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
    char* op_pos = NULL;
    size_t op_len = 0;
    sequence_op_t op_type = SEQ_NONE;

    while(*cursor != '\0') {
      if(cursor[0] == '&' && cursor[1] == '&') {
        op_pos = cursor;
        op_type = SEQ_AND;
        op_len = 2;
        break;
      }
      if(cursor[0] == '|' && cursor[1] == '|') {
        op_pos = cursor;
        op_type = SEQ_OR;
        op_len = 2;
        break;
      }
      if(cursor[0] == ';') {
        op_pos = cursor;
        op_type = SEQ_SEMI;
        op_len = 1;
        break;
      }
      cursor++;
    }

    if(op_pos != NULL) {
      *op_pos = '\0';
      if(op_len == 2) {
        op_pos[1] = ' ';
      }
    }

    rtrim(segment_start);
    parts[count++] = segment_start;

    if(op_pos == NULL) {
      break;
    }

    if(ops != NULL) {
      ops[count - 1] = op_type;
    }

    cursor = op_pos + op_len;
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

  char command_text[MOSH_MAX_LINE_LENGTH];
  str_copy(command_text, sizeof(command_text), line);
  bool background = strip_background_marker(command_text);
  if(background) {
    strip_background_marker(line);
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

  return execute_pipeline(command_text, segments, segment_count, background);
}

static int launch_command(char* line) {
  if(line == NULL) {
    shell_set_status_code(0);
    return 0;
  }

  char raw_buffer[MOSH_MAX_FUNCTION_BODY];
  str_copy(raw_buffer, sizeof(raw_buffer), line);

  char* trimmed_raw = ltrim(raw_buffer);

  if(trimmed_raw[0] != '\0' &&
     (shell_starts_with_keyword(trimmed_raw, "if") ||
      shell_starts_with_keyword(trimmed_raw, "while") ||
      shell_starts_with_keyword(trimmed_raw, "for") ||
      shell_starts_with_keyword(trimmed_raw, "function") ||
      shell_is_inline_function_signature(trimmed_raw))) {
    char working_segment[MOSH_MAX_FUNCTION_BODY];
    if(strchr(trimmed_raw, '$') != NULL) {
      char expanded_segment[MOSH_MAX_FUNCTION_BODY];
      write_str(STDERR_FILENO, "[mosh] expand ctx=launch_command:trimmed\n");
      if(!shell_expand_variables(trimmed_raw, expanded_segment, sizeof(expanded_segment))) {
        write_str(STDOUT_FILENO, "mosh: expansion too long\n");
        shell_set_status_code(1);
        return encode_raw_status_from_code(shell_last_status);
      }
      str_copy(working_segment, sizeof(working_segment), expanded_segment);
    } else {
      str_copy(working_segment, sizeof(working_segment), trimmed_raw);
    }
    int builtin_status = 0;
    if(handle_builtin(trimmed_raw, working_segment, &builtin_status)) {
      return builtin_status;
    }
  }

  char* raw_parts[MOSH_MAX_SEQUENCES];
  sequence_op_t ops[MOSH_MAX_SEQUENCES] = { SEQ_NONE };
  size_t count = split_sequence(trimmed_raw, raw_parts, ops, MOSH_MAX_SEQUENCES);
  if(count == 0) {
    shell_set_status_code(0);
    return 0;
  }

  int last_status = 0;

  for(size_t i = 0; i < count; i++) {
    char raw_segment[MOSH_MAX_FUNCTION_BODY];
    str_copy(raw_segment, sizeof(raw_segment), ltrim(raw_parts[i]));
    if(raw_segment[0] == '\0') {
      write_str(STDOUT_FILENO, "mosh: syntax error\n");
      shell_set_status_code(1);
      return encode_raw_status_from_code(shell_last_status);
    }

    write_str(STDERR_FILENO, "[mosh] segment raw=");
    if(debug_ptr_readable(raw_segment)) {
      write_bytes(STDERR_FILENO, raw_segment, str_len(raw_segment));
    } else {
      write_str(STDERR_FILENO, "<invalid>");
    }
    write_str(STDERR_FILENO, "\n");
    debug_dump_bytes("[mosh] segment raw", raw_segment);

    char working_segment[MOSH_MAX_FUNCTION_BODY];
    if(strchr(raw_segment, '$') != NULL) {
      write_str(STDERR_FILENO, "[mosh] expand ctx=launch_command:segment\n");
      char expanded_segment[MOSH_MAX_FUNCTION_BODY];
      if(!shell_expand_variables(raw_segment, expanded_segment, sizeof(expanded_segment))) {
        write_str(STDOUT_FILENO, "mosh: expansion too long\n");
        shell_set_status_code(1);
        return encode_raw_status_from_code(shell_last_status);
      }
      str_copy(working_segment, sizeof(working_segment), expanded_segment);
    } else {
      str_copy(working_segment, sizeof(working_segment), raw_segment);
    }

    int builtin_status = 0;
    if(handle_builtin(raw_segment, working_segment, &builtin_status)) {
      last_status = builtin_status;
    } else {
      last_status = launch_pipeline(working_segment);
      shell_set_status_from_raw(last_status);
    }

    if(i < count - 1) {
      sequence_op_t op = ops[i];
      if(op == SEQ_AND) {
        if(last_status != 0) {
          break;
        }
      } else if(op == SEQ_OR) {
        if(last_status == 0) {
          break;
        }
      } else if(op == SEQ_NONE) {
        if(last_status != 0) {
          break;
        }
      } else if(op == SEQ_SEMI) {
        continue;
      }
    }
  }

  return last_status;
}

static const char* shell_prompt(void) {
  static char prompt[MOSH_MAX_PATH + 32];
  const char prefix[] = "mosh:";
  const char suffix[] = "> ";
  const char* dir = current_directory;

  if(dir == NULL || dir[0] == '\0') {
    dir = "/";
  }

  size_t offset = 0;

  {
    const char fallback_time[] = "--:--:--";
    const char* time_src = fallback_time;
    char time_buf[sizeof(fallback_time) + 11];
    time_t now = time(NULL);
    if(now != (time_t)-1) {
      const struct tm* tm_ptr = localtime(&now);
      if(tm_ptr != NULL) {
        int p = 0;
        struct tm tm_copy = *tm_ptr;
        time_buf[p++] = '\x1b';
        time_buf[p++] = '[';
        time_buf[p++] = '9';
        time_buf[p++] = '2';
        time_buf[p++] = 'm';
        if(tm_copy.tm_hour >= 0 && tm_copy.tm_hour < 24 && tm_copy.tm_min >= 0 && tm_copy.tm_min < 60 &&
           tm_copy.tm_sec >= 0 && tm_copy.tm_sec < 60) {
          time_buf[p++] = (char)('0' + (tm_copy.tm_hour / 10));
          time_buf[p++] = (char)('0' + (tm_copy.tm_hour % 10));
          time_buf[p++] = ':';
          time_buf[p++] = (char)('0' + (tm_copy.tm_min / 10));
          time_buf[p++] = (char)('0' + (tm_copy.tm_min % 10));
          time_buf[p++] = ':';
          time_buf[p++] = (char)('0' + (tm_copy.tm_sec / 10));
          time_buf[p++] = (char)('0' + (tm_copy.tm_sec % 10));
          time_src = time_buf;
        }
        time_buf[p++] = '\x1b';
        time_buf[p++] = '[';
        time_buf[p++] = '3';
        time_buf[p++] = '9';
        time_buf[p++] = 'm';
        time_buf[p++] = '\0';
      }
    }

    for(size_t i = 0; time_src[i] != '\0' && offset < sizeof(prompt) - 1; i++) {
      prompt[offset++] = time_src[i];
    }
    if(offset < sizeof(prompt) - 1) {
      prompt[offset++] = ' ';
    }
  }

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
    jobs_poll_updates(true);
    const char* prompt = shell_prompt();
    size_t length = read_line(prompt, line_buffer, sizeof(line_buffer));

    if(line_buffer[0] == '\0') {
      continue;
    }

    char original_line[MOSH_MAX_LINE_LENGTH];
    str_copy(original_line, sizeof(original_line), line_buffer);
    history_add(original_line);

    launch_command(line_buffer);
    jobs_poll_updates(true);
  }
}

#ifndef MOSH_TEST
int main(int argc, char** argv, char** envp) {
  (void)argc;
  const uintptr_t MIN_USER_PTR = 0x1000u;
  uintptr_t argv_raw = (uintptr_t)argv;
  uintptr_t envp_raw = (uintptr_t)envp;

  const char* argv0 = "(null)";
  uintptr_t argv0_raw = 0u;
  if(argv != NULL && argv_raw >= MIN_USER_PTR) {
    const char* candidate = argv[0];
    argv0_raw = (uintptr_t)candidate;
    if(candidate != NULL && argv0_raw >= MIN_USER_PTR) {
      argv0 = candidate;
    }
  }

  bool envp_valid = (envp != NULL) && (envp_raw >= MIN_USER_PTR);
  uintptr_t env0_raw = 0u;
  const char* env0 = "(null)";
  if(envp_valid) {
    const char* candidate = envp[0];
    env0_raw = (uintptr_t)candidate;
    if(candidate != NULL) {
      if(env0_raw >= MIN_USER_PTR) {
        env0 = candidate;
      } else {
        envp_valid = false;
        env0 = "(invalid)";
      }
    }
  }
  if(!envp_valid) {
    envp = NULL;
  }

  {
    char buffer[224];
    size_t pos = 0u;
    pos = debug_append_str(buffer, pos, sizeof(buffer), "[mosh] main entry argv=");
    pos = debug_append_hex(buffer, pos, sizeof(buffer), argv_raw);
    pos = debug_append_str(buffer, pos, sizeof(buffer), " argv0=");
    pos = debug_append_hex(buffer, pos, sizeof(buffer), argv0_raw);
    pos = debug_append_str(buffer, pos, sizeof(buffer), " envp=");
    pos = debug_append_hex(buffer, pos, sizeof(buffer), envp_raw);
    pos = debug_append_str(buffer, pos, sizeof(buffer), " env0=");
    pos = debug_append_hex(buffer, pos, sizeof(buffer), env0_raw);
    pos = debug_append_str(buffer, pos, sizeof(buffer), " envp_valid=");
    pos = debug_append_str(buffer, pos, sizeof(buffer), envp_valid ? "yes" : "no");
    if(pos < sizeof(buffer)) {
      buffer[pos++] = '\n';
    }
    (void)write(STDERR_FILENO, buffer, pos);
    write_str(STDERR_FILENO, "[mosh] argv0: ");
    write_str(STDERR_FILENO, argv0);
    write_str(STDERR_FILENO, "\n");
    write_str(STDERR_FILENO, "[mosh] env0: ");
    write_str(STDERR_FILENO, env0);
    write_str(STDERR_FILENO, "\n");
  }
  env_release_heap_entries();
  process_envp = (envp != NULL) ? envp : fallback_envp;
  str_copy(current_directory, sizeof(current_directory), "/");
  const char* initial_pwd = env_get("PWD");
  if(initial_pwd != NULL && initial_pwd[0] != '\0') {
    if(!normalize_path("/", initial_pwd, current_directory, sizeof(current_directory))) {
      str_copy(current_directory, sizeof(current_directory), "/");
    }
  }
  env_set("PWD", current_directory);
  shell_install_signal_handlers();
  history_reset();
  shell_run_startup_script();
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
