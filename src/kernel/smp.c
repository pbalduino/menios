#include <boot/limine.h>
#include <kernel/console.h>
#include <kernel/kernel.h>
#include <kernel/smp.h>

static volatile struct limine_smp_request smp_request = {
  .id = LIMINE_SMP_REQUEST,
  .revision = 0,
  .flags = 1
};

void smp_init() {
  printf("- Starting SMP...");

  if (smp_request.response == NULL || smp_request.response->cpu_count < 1) {
    printf(" FAILED. No CPU found.\n");
    hcf();
  }

  printf("OK. %lu available CPUs.\n", smp_request.response->cpu_count);

}
