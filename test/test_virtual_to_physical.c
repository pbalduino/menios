#include <kernel/pmm.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

static pml4_t* pml4_root;
static page_directory_pointer_t* pdpt_level;
static page_directory_t* pd_level;
static page_table_t* pt_level;

static void* alloc_page_table(void) {
  void* table = aligned_alloc(PAGE_SIZE, PAGE_SIZE);
  if(table != NULL) {
    memset(table, 0, PAGE_SIZE);
  }
  return table;
}

void setUp(void) {
  pml4_root = alloc_page_table();
  pdpt_level = alloc_page_table();
  pd_level = alloc_page_table();
  pt_level = alloc_page_table();

  TEST_ASSERT_NOT_NULL(pml4_root);
  TEST_ASSERT_NOT_NULL(pdpt_level);
  TEST_ASSERT_NOT_NULL(pd_level);
  TEST_ASSERT_NOT_NULL(pt_level);

  pmm_set_kernel_offset(0);
  pmm_set_pagetable_root((virt_addr_t)pml4_root);

  page_map_l4_entry_t* pml4_entry = &pml4_root->entries[0];
  pml4_entry->present = 1;
  pml4_entry->writable = 1;
  pml4_entry->page_directory_base = ((uintptr_t)pdpt_level) >> 12;

  page_directory_pointer_entry_t* pdpt_entry = &pdpt_level->entries[0];
  pdpt_entry->present = 1;
  pdpt_entry->writable = 1;
  pdpt_entry->page_directory_base = ((uintptr_t)pd_level) >> 12;

  page_directory_entry_t* pd_entry = &pd_level->entries[0];
  pd_entry->present = 1;
  pd_entry->writable = 1;
  pd_entry->large_page = 0;
  pd_entry->page_table_base = ((uintptr_t)pt_level) >> 12;
}

void tearDown(void) {
  free(pml4_root);
  free(pdpt_level);
  free(pd_level);
  free(pt_level);
}

void test_virtual_to_physical_translates_4k_mapping(void) {
  const virt_addr_t virtual_address = 0x0000000000002345ULL;
  const phys_addr_t frame_base = 0x0000000000ABC000ULL;
  const uint16_t pt_index = (virtual_address >> 12) & 0x1ff;

  page_table_entry_t* pt_entry = &pt_level->entries[pt_index];
  pt_entry->present = 1;
  pt_entry->writable = 1;
  pt_entry->frame = frame_base >> 12;

  phys_addr_t translated = virtual_to_physical(virtual_address);
  phys_addr_t expected = (frame_base & ~((phys_addr_t)PAGE_SIZE - 1)) | (virtual_address & (PAGE_SIZE - 1));

  TEST_ASSERT_EQUAL_UINT64(expected, translated);
}

void test_virtual_to_physical_translates_2m_large_page(void) {
  const virt_addr_t virtual_address = (1ULL << 21) + 0x5678ULL;
  const uint16_t pd_index = (virtual_address >> 21) & 0x1ff;
  const phys_addr_t large_page_base = 0x0000000020000000ULL;

  page_directory_entry_t* pd_entry = &pd_level->entries[pd_index];
  pd_entry->present = 1;
  pd_entry->writable = 1;
  pd_entry->large_page = 1;
  pd_entry->page_table_base = large_page_base >> 12;

  phys_addr_t translated = virtual_to_physical(virtual_address);
  phys_addr_t expected = (large_page_base & ~(((phys_addr_t)1 << 21) - 1)) |
                         (virtual_address & (((phys_addr_t)1 << 21) - 1));

  TEST_ASSERT_EQUAL_UINT64(expected, translated);
}

void test_virtual_to_physical_returns_invalid_for_absent_pml4_entry(void) {
  const virt_addr_t virtual_address = 0x0000800000000000ULL;

  TEST_ASSERT_EQUAL_UINT64(PHYS_ADDR_INVALID, virtual_to_physical(virtual_address));
}

void test_virtual_to_physical_returns_invalid_for_missing_pte(void) {
  const virt_addr_t virtual_address = 0x0000000000001000ULL;

  TEST_ASSERT_EQUAL_UINT64(PHYS_ADDR_INVALID, virtual_to_physical(virtual_address));
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_virtual_to_physical_translates_4k_mapping);
  RUN_TEST(test_virtual_to_physical_translates_2m_large_page);
  RUN_TEST(test_virtual_to_physical_returns_invalid_for_absent_pml4_entry);
  RUN_TEST(test_virtual_to_physical_returns_invalid_for_missing_pte);

  return UNITY_END();
}
