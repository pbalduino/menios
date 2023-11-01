#include <assert.h>
#include <kernel/kernel.h>
#include <stdio.h>

void _panic(const char* a, int b, const char* c, ... ) {
  printf("%s: %d - %s", a, b, c);
  hcf();
}
