#include <kernel/mem.h>
#include <kernel/core/services.h>
#include <stdio.h>

void init_services(void) {
  printf("- Initing background services.");

  // init_memory_compactor();

  printf("OK\n");
}
