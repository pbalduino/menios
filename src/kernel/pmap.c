#include <boot/limine.h>
#include <kernel/console.h>
#include <kernel/pmap.h>
#include <kernel/kernel.h>

static volatile struct limine_memmap_request memmap_request = {
  .id = LIMINE_MEMMAP_REQUEST,
  .revision = 0
};

static struct limine_memmap_response *memmap;

void mem_init() {
  printf("- Starting memory management");
  if(memmap_request.response == NULL || memmap_request.response->entry_count < 1) {
    printf(" > Error mapping the memory. Halting.");
    hcf();
  }
  printf("...\n");

  // for(uint64_t p = 0; p < memmap->entry_count; p++) {
  printf("addr: %lu | ", (uintptr_t)memmap->entries[0]);
  printf("%lu | ", memmap->entries[0]->length);
  // }


}
