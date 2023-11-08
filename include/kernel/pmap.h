#ifndef MENIOS_INCLUDE_KERNEL_PMAP_H
#define MENIOS_INCLUDE_KERNEL_PMAP_H

#include <types.h>

// Page Map Level 4 (PML4) Entry
typedef struct {
  uint64_t present:              1;  // Page present in memory
  uint64_t writable:             1;  // Writable (if 0, read-only)
  uint64_t user:                 1;  // User mode accessible
  uint64_t write_through:        1;  // Write-through caching
  uint64_t cache_disable:        1;  // Disable caching
  uint64_t accessed:             1;   // Page accessed (set by CPU)
  uint64_t reserved1:            1;  // Reserved
  uint64_t large_page:           1; // 1 indicates a 1 GB page (instead of PDE)
  uint64_t zero:                 1;       // Reserved, set to 0
  uint64_t available:            3;  // Available for use by the OS
  uint64_t page_directory_base: 40; // Physical address of Page Directory
  uint64_t reserved2:           11; // Reserved
  uint64_t no_execute:           1; // Disable execution (NX bit)
} page_map_l4_entry_t;

typedef struct {
    page_map_l4_entry_t entries[512];      // Array of PML4 Entries
} pml4_t __attribute__((aligned(4096)));

typedef struct {
  uint64_t present : 1;    // Page present in memory
  uint64_t writable : 1;   // Writable (if 0, read-only)
  uint64_t user : 1;       // User mode accessible
  uint64_t write_through : 1;  // Write-through caching
  uint64_t cache_disable : 1;  // Disable caching
  uint64_t accessed : 1;   // Page accessed (set by CPU)
  uint64_t reserved : 1;   // Reserved
  uint64_t large_page : 1; // 1 indicates a 2 MB page (instead of PTE)
  uint64_t global : 1;     // Global page (ignored in page directory)
  uint64_t available : 3;  // Available for use by the OS
  uint64_t page_table_base : 40; // Physical address of Page Table
  uint64_t reserved2 : 11; // Reserved
  uint64_t no_execute : 1; // Disable execution (NX bit)
} page_directory_entry_t;

typedef struct {
  page_directory_entry_t entries[512];        // Array of Page Directory Entries
} page_directory_t __attribute__((aligned(4096)));

typedef struct {
  uint64_t present : 1;    // Page present in memory
  uint64_t writable : 1;   // Writable (if 0, read-only)
  uint64_t user : 1;       // User mode accessible
  uint64_t write_through : 1;  // Write-through caching
  uint64_t cache_disable : 1;  // Disable caching
  uint64_t accessed : 1;   // Page accessed (set by CPU)
  uint64_t dirty : 1;      // Page written to (set by CPU)
  uint64_t reserved : 1;   // Reserved
  uint64_t global : 1;     // Global page (ignored in page table)
  uint64_t available : 3;  // Available for use by the OS
  uint64_t frame : 40;     // Frame address (physical page number)
  uint64_t reserved2 : 11; // Reserved
  uint64_t no_execute : 1; // Disable execution (NX bit)
} page_table_entry_t;

typedef struct {
  page_table_entry_t entries[512];        // Array of Page Table Entries
} page_table_t __attribute__((aligned(4096)));

void pmap_init();

typedef struct {
  uint64_t present : 1;    // Page present in memory
  uint64_t writable : 1;   // Writable (if 0, read-only)
  uint64_t user : 1;       // User mode accessible
  uint64_t write_through : 1;  // Write-through caching
  uint64_t cache_disable : 1;  // Disable caching
  uint64_t accessed : 1;   // Page accessed (set by CPU)
  uint64_t reserved : 1;   // Reserved
  uint64_t large_page : 1; // 1 indicates a 1 GB page (instead of PDE)
  uint64_t zero : 1;       // Reserved, set to 0
  uint64_t available : 3;  // Available for use by the OS
  uint64_t page_directory_base : 40; // Physical address of Page Directory
  uint64_t reserved2 : 11; // Reserved
  uint64_t no_execute : 1; // Disable execution (NX bit)
} page_directory_pointer_entry_t;

typedef struct {
  page_directory_pointer_entry_t entries[512];        // Array of Page Directory Entries
} page_directory_pointer_t __attribute__((aligned(4096)));

#endif

// 0xfffffffff30fc6c0
// 0xffffffffc13d1000
// 0x000000007ff5e000
// 0x0000079da8131000
