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

void read_mem(mem_t* mem) {
	// Use CMOS calls to measure available base & extended memory.
	// (CMOS calls return results in kilobytes.)
	mem->basemem = nvram_read(NVRAM_BASELO);
	mem->extmem = nvram_read(NVRAM_EXTLO);
	mem->ext16mem = nvram_read(NVRAM_EXT16LO) * 64;

	if (mem->ext16mem) {
		mem->totalmem = 16 * 1024 + mem->ext16mem;
	} else if (mem->extmem) {
		mem->totalmem = 1024 + mem->extmem;
	} else {
		mem->totalmem = mem->basemem;
  }
}

static void i386_detect_memory() {
	mem_t mem;

	read_mem(&mem);

	npages = mem.totalmem / (PGSIZE / 1024);
	npages_basemem = mem.basemem / (PGSIZE / 1024);

	printf("\nPhysical memory:   %uMB available\n  Base memory:     %uKB\n  Extended memory: %uMB\n",
		mem.totalmem / 1024, mem.basemem, (mem.totalmem - mem.basemem) / 1024);
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
