#ifndef MENIOS_INCLUDE_MMU_H
#define MENIOS_INCLUDE_MMU_H

#define PGSIZE		4096		// bytes mapped by a page

#define PGOFF(la)	(((uintptr_t) (la)) & 0xFFF)

#endif
