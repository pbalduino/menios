#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int resolve_path(const char* input) {
  if(input == NULL || *input == '\0') {
    fprintf(stderr, "realpath: invalid path\n");
    return 1;
  }

  char* resolved = realpath(input, NULL);
  if(resolved == NULL) {
    int err = errno;
    const char* message = strerror(err);
    if(message == NULL) {
      message = "Unknown error";
    }
    fprintf(stderr, "realpath: %s: %s\n", input, message);
    return 1;
  }

  printf("%s\n", resolved);
  free(resolved);
  return 0;
}

int main(int argc, char** argv) {
  int exit_code = 0;

  if(argc <= 1) {
    exit_code = resolve_path(".");
  } else {
    for(int i = 1; i < argc; i++) {
      if(resolve_path(argv[i]) != 0) {
        exit_code = 1;
      }
    }
  }

  return exit_code;
}
