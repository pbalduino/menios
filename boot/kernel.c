#include <stdio.h>
#include <stdlib.h>
#include <kernel/pmap.h>

void kernel_start() {
  const char* hello = "\nWelcome to MeniOS 0.0.1\n\0";
  puts(hello);

  // page memory
  init_memory();

  // enumerate drives

  // start extra cores
}
