#include <kernel/mem.h>
#include <kernel/services.h>
#include <stdio.h>

void init_services() {
  printf("- Initing background services.");

  // init_memory_compactor();

  printf("OK\n");
}