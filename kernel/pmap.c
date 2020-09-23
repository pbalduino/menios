#include <kernel/rtclock.h>
#include <kernel/pmap.h>

#include <assert.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
#include <x86.h>

// These variables are set by i386_detect_memory()
size_t npages;			// Amount of physical memory (in pages)
static size_t npages_basemem;	// Amount of base memory (in pages)

uint32_t page_directory[1024] __attribute__((aligned(4096)));

static int nvram_read(int r) {
	return mc146818_read(r) | (mc146818_read(r + 1) << 8);
}

static void i386_detect_memory() {
	size_t basemem, extmem, ext16mem, totalmem;

	// Use CMOS calls to measure available base & extended memory.
	// (CMOS calls return results in kilobytes.)
	basemem = nvram_read(NVRAM_BASELO);
	extmem = nvram_read(NVRAM_EXTLO);
	ext16mem = nvram_read(NVRAM_EXT16LO) * 64;

	// Calculate the number of physical pages available in both base
	// and extended memory.
	if (ext16mem) {
		totalmem = 16 * 1024 + ext16mem;
	} else if (extmem) {
		totalmem = 1 * 1024 + extmem;
	} else {
		totalmem = basemem;
  }

	npages = totalmem / (PGSIZE / 1024);
	npages_basemem = basemem / (PGSIZE / 1024);

	printf("\nPhysical memory:   %uMB available\n  Base memory:     %uKB\n  Extended memory: %uMB\n\0",
		totalmem / 1024, basemem, (totalmem - basemem) / 1024);
	io_wait();
}

static void init_paging() {
  for(int i = 0; i < 1024; i++) {
    // This sets the following flags to the pages:
    //   Supervisor: Only kernel-mode can access them
    //   Write Enabled: It can be both read from and written to
    //   Not Present: The page table is not present
    page_directory[i] = 0x00000002;
  }

  load_page_directory(page_directory);
  enable_paging();
  printf("Memory paging is enabled\n");
}

void init_memory() {
  printf("Initalizing memory\n");
	io_wait();

	i386_detect_memory();

  init_paging();
}

void test_paging() {

}
