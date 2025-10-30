#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* program_name = "head";

static void print_usage(void) {
  fprintf(stderr, "%s: usage: head [-n lines] [file ...]\n", program_name);
}

static bool parse_count(const char* text, long* out_value) {
  if(text == NULL || out_value == NULL) {
    return false;
  }

  char* end = NULL;
  long value = strtol(text, &end, 10);
  if(end == text || (end != NULL && *end != '\0')) {
    return false;
  }
  if(value < 0) {
    return false;
  }

  *out_value = value;
  return true;
}

static int dump_stream(FILE* stream, const char* label, long lines_to_print) {
  if(stream == NULL) {
    return 0;
  }

  if(lines_to_print <= 0) {
    return 0;
  }

  char buffer[512];
  long remaining = lines_to_print;

  while(remaining > 0) {
    if(fgets(buffer, (int)sizeof(buffer), stream) == NULL) {
      if(ferror(stream)) {
        fprintf(stderr, "%s: read error on %s\n", program_name, label);
        return -1;
      }
      break;
    }
    fputs(buffer, stdout);
    remaining--;
  }

  return 0;
}

static int run_head_on_path(const char* path, long lines_to_print, bool print_header, bool* first_output) {
  const char* label = path ? path : "stdin";
  FILE* file = NULL;

  if(path == NULL || strcmp(path, "-") == 0) {
    file = stdin;
  } else {
    file = fopen(path, "r");
    if(file == NULL) {
      fprintf(stderr, "%s: cannot open %s: %s\n", program_name, path, strerror(errno));
      return 1;
    }
  }

  int result = 0;

  if(print_header) {
    if(!*first_output) {
      fputc('\n', stdout);
    }
    fprintf(stdout, "==> %s <==\n", (file == stdin) ? "stdin" : path);
  }

  if(dump_stream(file, label, lines_to_print) < 0) {
    result = 1;
  }

  if(file != NULL && file != stdin) {
    fclose(file);
  }

  *first_output = false;
  return result;
}

int main(int argc, char** argv) {
  if(argv != NULL && argv[0] != NULL) {
    program_name = argv[0];
  }

  long lines_to_print = 10;
  int arg_index = 1;
  int exit_code = 0;

  while(arg_index < argc) {
    const char* arg = argv[arg_index];
    if(arg == NULL) {
      arg_index++;
      continue;
    }

    if(strcmp(arg, "--") == 0) {
      arg_index++;
      break;
    }

    if(arg[0] != '-' || strcmp(arg, "-") == 0) {
      break;
    }

    if(strcmp(arg, "-n") == 0) {
      arg_index++;
      if(arg_index >= argc) {
        fprintf(stderr, "%s: option '-n' requires a number\n", program_name);
        print_usage();
        return 1;
      }
      const char* count_text = argv[arg_index];
      if(!parse_count(count_text, &lines_to_print)) {
        fprintf(stderr, "%s: invalid line count: %s\n", program_name, count_text);
        return 1;
      }
      arg_index++;
      continue;
    }

    if(arg[0] == '-' && arg[1] >= '0' && arg[1] <= '9') {
      if(!parse_count(arg + 1, &lines_to_print)) {
        fprintf(stderr, "%s: invalid line count: %s\n", program_name, arg);
        return 1;
      }
      arg_index++;
      continue;
    }

    fprintf(stderr, "%s: unknown option: %s\n", program_name, arg);
    print_usage();
    return 1;
  }

  if(lines_to_print < 0) {
    lines_to_print = 0;
  }

  if(arg_index >= argc) {
    bool first_output = false;
    return run_head_on_path(NULL, lines_to_print, false, &first_output);
  }

  bool first_output = true;
  for(int i = arg_index; i < argc; i++) {
    const char* path = argv[i];
    if(path == NULL) {
      continue;
    }

    bool print_header = ((argc - arg_index) > 1);
    if(run_head_on_path(path, lines_to_print, print_header, &first_output) != 0) {
      exit_code = 1;
    }
  }

  return exit_code;
}
