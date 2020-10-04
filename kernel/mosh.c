#include <kernel/dummy.h>
#include <kernel/pmap.h>
#include <stdio.h>
#include <string.h>

void mem() {
  mem_t mem;
  read_mem(&mem);

  printf("\n  Physical memory:   %uMB available\n  Base memory:     %uKB\n  Extended memory: %uMB",
    mem.totalmem / 1024, mem.basemem, (mem.totalmem - mem.basemem) / 1024);

}

void execute(const char* command) {
  if(strcmp("mem", command) == 0) {
    mem();
  } else {
    printf("\n-mosh: ");
    printf("%s: command not found", command);
  }
}
// mosh is the MeniOS Shell
void mosh() {
  char command[0];

  while(1) {
    printf("kernel# ");
    gets(command);

    if(strcmp(command, "") != 0) {
      execute(command);
    }
    putchar('\n');
    memset(command, 0, strlen(command));
  }
}
