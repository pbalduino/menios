#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <menios/syscall.h>
#include <menios/syscall_user.h>

static void print_usage(void) {
  fprintf(stderr, "usage: shutdown\n");
}

#ifndef SHUTDOWN_REQUEST_POWEROFF_DEFINED
__attribute__((weak)) long shutdown_request_poweroff(void) {
  return __menios_syscall0(SYS_SHUTDOWN);
}
#endif

int shutdown_main(int argc, char** argv) {
  if(argc > 1) {
    if(argc == 2 && argv[1] != NULL && strcmp(argv[1], "--help") == 0) {
      print_usage();
      return 0;
    }

    fprintf(stderr, "shutdown: unexpected argument\n");
    print_usage();
    return 1;
  }

  printf("shutdown: powering off...\n");

  long rc = shutdown_request_poweroff();
  if(rc < 0) {
    int err = (int)(-rc);
    errno = err;
    perror("shutdown");
    errno = err;
    return 1;
  }

  return 0;
}

#ifndef SHUTDOWN_TEST
int main(int argc, char** argv) {
  return shutdown_main(argc, argv);
}
#endif
