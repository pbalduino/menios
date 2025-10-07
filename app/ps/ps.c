#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <menios/syscall.h>
#include <menios/syscall_user.h>

#define PS_BUFFER_SIZE 1024

int main(void) {
  char buffer[PS_BUFFER_SIZE];
  long rc = __menios_syscall2(SYS_PROC_LIST, (long)buffer, (long)sizeof(buffer));
  if(rc < 0) {
    errno = (int)(-rc);
    perror("ps");
    return 1;
  }

  size_t length = (size_t)rc;
  if(length >= sizeof(buffer)) {
    length = sizeof(buffer) - 1;
  }
  buffer[length] = '\0';
  fputs(buffer, stdout);
  return 0;
}
