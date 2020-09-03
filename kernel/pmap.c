#include <mmu.h>
#include <stdio.h>

#include <kernel/rtclock.h>
#include <kernel/pmap.h>

// These variables are set by i386_detect_memory()
size_t npages;			// Amount of physical memory (in pages)
static size_t npages_basemem;	// Amount of base memory (in pages)

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
	if (ext16mem)
		totalmem = 16 * 1024 + ext16mem;
	else if (extmem)
		totalmem = 1 * 1024 + extmem;
	else
		totalmem = basemem;

	npages = totalmem / (PGSIZE / 1024);
	npages_basemem = basemem / (PGSIZE / 1024);

	printf("\nPhysical memory:   %uMB available\n  Base memory:     %uKB\n  Extended memory: %uMB\n\0",
		totalmem / 1024, basemem, (totalmem - basemem) / 1024);
}

void init_memory() {
	// uint32_t cr0;
	// size_t n;

	// Find out how much memory the machine has (npages & npages_basemem).
	i386_detect_memory();
}
