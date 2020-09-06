#ifndef MENIOS_INCLUDE_KERNEL_PMAP_H
#define MENIOS_INCLUDE_KERNEL_PMAP_H

// kernel/mem_page.s
extern void load_page_directory(unsigned int*);
extern void enable_paging();

// kernel/pmap.c
void init_memory();

#endif
