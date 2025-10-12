#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int dump_meminfo(void) {
  FILE* file = fopen("/proc/meminfo", "r");
  if(file == NULL) {
    perror("mem: failed to open /proc/meminfo");
    return 1;
  }

  char buffer[256];
  while(fgets(buffer, sizeof(buffer), file) != NULL) {
    fputs(buffer, stdout);
  }

  if(ferror(file) != 0) {
    perror("mem: error reading /proc/meminfo");
    fclose(file);
    return 1;
  }

  fclose(file);
  return 0;
}

int main(void) {
  return dump_meminfo();
}
