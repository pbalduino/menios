#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <kernel/pmap.h>

void kernel_start() {
  const char* hello = "\nWelcome to MeniOS 0.0.1\n\n";
  puts(hello);

  // page memory
  init_memory();

  // init_ports();

  // init_storage();

  printf("\n");

  // enumerate drives

  // start extra cores

  warn("A warning won't do harm");

  printf("\n");

  panic("Calling panic for testing purposes");
}
