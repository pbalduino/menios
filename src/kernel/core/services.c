#include <kernel/mem.h>
#include <kernel/core/services.h>
#include <stdio.h>

void services_initialize(void) {
  printf("- Initing background services.");

  // mem_compactor_initialize();

  printf("OK\n");
}
