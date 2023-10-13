#ifndef MENIOS_INCLUDE_KERNEL_PMAP_H
#define MENIOS_INCLUDE_KERNEL_PMAP_H

#include <types.h>

#define PD_PRESENT          0x0001
#define PD_WRITABLE         0x0002
#define PD_USER_ACCESS      0x0004
#define PD_DISABLED_CACHE   0x0008
#define PD_ACCESSED         0x0010
#define PD_ZERO             0x0020
#define PD_BIG_PAGE         0x0040
#define PD_IGNORED          0x0080

typedef struct {
  bool present        : 1;
  bool writable       : 1;
  bool user_access    : 1;
  bool write_thru     : 1;
  bool disabled_cache : 1;
  bool accessed       : 1;
  uint8_t zero        : 1;
  bool big_page       : 1;
  uint8_t ignored     : 1;
  uint8_t cargo       : 3;
  uint32_t page_table : 20;
} __attribute__((packed)) page_dir_entry_t;

typedef struct {
  bool present        : 1;
  bool writable       : 1;
  bool user_access    : 1;
  bool write_thru     : 1;
  bool disabled_cache : 1;
  bool accessed       : 1;
  bool dirty          : 1;
  uint8_t zero        : 1;
  bool global         : 1;
  uint8_t cargo       : 3;
  uint32_t phys_addr  : 20;
} __attribute__((packed)) page_table_entry_t;

extern void init_paging(uint32_t page_dir);

void mem_init();

#endif
