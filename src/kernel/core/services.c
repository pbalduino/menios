#include <kernel/mem.h>
#include <kernel/core/services.h>
#include <stdio.h>

void services_init(void) {
  printf("- Initing background services.");

  // mem_compactor_init();

  printf("OK\n");
}
