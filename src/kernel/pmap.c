#include <boot/limine.h>
#include <kernel/console.h>
#include <kernel/pmap.h>
#include <kernel/kernel.h>

static volatile struct limine_memmap_request memmap_request = {
  .id = LIMINE_MEMMAP_REQUEST,
  .revision = 0
};

static struct limine_memmap_response *memmap;
static uint64_t mem_total = 0;
static uint64_t mem_available = 0;
static uint64_t mem_framebuffer = 0;

void mem_init() {
  printf("- Starting memory management");
  if(memmap_request.response == NULL || memmap_request.response->entry_count < 1) {
    printf(" > Error mapping the memory. Halting.");
    hcf();
  }

  memmap = memmap_request.response;

  printf(". %lu entries found..\n", memmap->entry_count);

  for(uint64_t e = 0; e < memmap->entry_count; e++) {
    printf("  Entry %lu:  base: %lx - size: %lu - type: %lu\n", e, memmap->entries[e]->base, memmap->entries[e]->length, memmap->entries[e]->type);
    switch (memmap->entries[e]->type) {
    case LIMINE_MEMMAP_USABLE:
      mem_available += memmap->entries[e]->length;
      break;
    case LIMINE_MEMMAP_FRAMEBUFFER:
    mem_framebuffer += memmap->entries[e]->length;
      break;
    default:
      break;
    }

    mem_total += memmap->entries[e]->length;
  }

  printf("  Total: %luMB - available: %luMB - video: %luMB\n", mem_total / (1024 * 1024), mem_available / (1024 * 1024), mem_framebuffer / (1024 * 1024));

}
