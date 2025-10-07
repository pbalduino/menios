#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <menios/syscall.h>
#include <menios/syscall_user.h>

int main(int argc, char** argv) {
  if(argc < 2 || argv[1] == NULL) {
    fprintf(stderr, "usage: kill <pid> [exit_code]\n");
    return 1;
  }

  char* endptr = NULL;
  long pid = strtol(argv[1], &endptr, 10);
  if(endptr == argv[1] || *endptr != '\0' || pid < 0) {
    fprintf(stderr, "kill: invalid pid '%s'\n", argv[1]);
    return 1;
  }

  long code = 0;
  if(argc > 2 && argv[2] != NULL) {
    char* code_end = NULL;
    code = strtol(argv[2], &code_end, 10);
    if(code_end == argv[2] || *code_end != '\0') {
      fprintf(stderr, "kill: invalid exit code '%s'\n", argv[2]);
      return 1;
    }
  }

  long rc = __menios_syscall2(SYS_PROC_KILL, pid, code);
  if(rc < 0) {
    perror("kill");
    return 1;
  }

  return 0;
}
