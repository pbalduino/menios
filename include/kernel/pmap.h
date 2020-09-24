#ifndef MENIOS_INCLUDE_KERNEL_PMAP_H
#define MENIOS_INCLUDE_KERNEL_PMAP_H

#include <types.h>
// kernel/mem_page.s
extern void load_page_directory(unsigned int*);
extern void enable_paging();

typedef struct {
	size_t basemem, extmem, ext16mem, totalmem;
} mem_t;
// kernel/pmap.c
void init_memory();
void read_mem(mem_t* mem);

#endif
