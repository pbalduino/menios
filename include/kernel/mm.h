#ifndef MENIOS_INCLUDE_KERNEL_MM_H
#define MENIOS_INCLUDE_KERNEL_MM_H

#define PD_PRESENT          0x0001
#define PD_WRITABLE         0x0002
#define PD_USER_ACCESS      0x0004
#define PD_DISABLED_CACHE   0x0008
#define PD_ACCESSED         0x0010
#define PD_ZERO             0x0020
#define PD_BIG_PAGE         0x0040
#define PD_IGNORED          0x0080

void mm_init();

#endif
