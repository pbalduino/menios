#include <stdio.h>
#include <string.h>

// mosh is the MeniOS Shell
void mosh() {
  char command[0];

  while(1) {
    printf("kernel# ");
    gets(command);

    if(strcmp(command, "") != 0) {
      printf("\n-mosh: ");
      printf("%s: command not found", command);
    }
    putchar('\n');
    memset(command, 0, strlen(command));
  }
}
