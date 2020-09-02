#include <stdio.h>
#include <stdlib.h>
#include <x86.h>

void kernel_start() {
  const char* hello = "\nWelcome to MeniOS 0.0.1\n\0";
  puts(hello);

  uint32_t info = 0;
  uint32_t eaxp = 0;
  uint32_t ebxp = 0;
  uint32_t ecxp = 0;
  uint32_t edxp = 0;

  puts("CPUID\n");

  cpuid(info, &eaxp, &ebxp, &ecxp, &edxp);

  printf("printf: [%c] - [%c] - [%d] - [%d] - ok\n\0", (eaxp >> 8) & 0xff, ebxp, ecxp, edxp);

  // enumerate drives
}
