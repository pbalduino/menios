#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/pci.h>
#include <kernel/pmap.h>
#include <kernel/storage.h>

void kernel_start() {
  const char* hello = "\nWelcome to MeniOS 0.0.1\n\n";
  puts(hello);

  init_pci();

  init_storage();

  init_memory();

  // printf("\n");

  // start extra cores
  // warn("A warning won't do harm");

  // printf("\n");

  // panic("Calling panic for testing purposes");
}
