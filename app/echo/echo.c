#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  if(argc <= 1) {
    putchar('\n');
    return 0;
  }

  for(int i = 1; i < argc; i++) {
    if(i > 1) {
      putchar(' ');
    }
    const char* text = argv[i];
    if(text != NULL) {
      fputs(text, stdout);
    }
  }
  putchar('\n');
  return 0;
}
