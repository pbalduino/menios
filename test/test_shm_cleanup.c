#include "unity.h"

#include <kernel/shm.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <kernel/pmm.h>
#include <kernel/heap.h>

#include <stdint.h>
#include <string.h>

static proc_info_t parent_proc;
static proc_info_t child_proc;

static phys_addr_t fake_alloc(size_t page_count) {
  size_t bytes = page_count * PAGE_SIZE;
  void* block = kmalloc(bytes);
  if(block != NULL) {
    memset(block, 0, bytes);
  }
  return (phys_addr_t)(uintptr_t)block;
}

static void fake_free(phys_addr_t base, size_t page_count) {
  (void)page_count;
  void* block = (void*)(uintptr_t)base;
  kfree(block);
}

static void destroy_region(int shmid) {
  shm_region_t* region = shm_region_get_by_id(shmid);
  if(region == NULL) {
    return;
  }
  shm_region_set_marked_for_removal(region, true);
  shm_region_unref(region);
}

static virt_addr_t reserve_base(proc_info_p proc, size_t length) {
  virt_addr_t base = proc->mmap_next ? proc->mmap_next : proc->mmap_base;
  base = (base + PAGE_SIZE - 1) & ~((virt_addr_t)PAGE_SIZE - 1);
  virt_addr_t end = base + length;
  proc->mmap_next = end;
  return base;
}

static void init_proc(proc_info_p proc) {
  memset(proc, 0, sizeof(*proc));
  proc->mmap_base = 0x100000000ULL;
  proc->mmap_limit = proc->mmap_base + 0x010000000ULL;
  proc->mmap_next = proc->mmap_base;
  proc->cwd[0] = '/';
  proc->cwd[1] = '\0';
  proc->cwd_len = 1;
}

void setUp(void) {
  shm_manager_init();
  shm_set_allocator(fake_alloc, fake_free);
  init_proc(&parent_proc);
  init_proc(&child_proc);
}

void tearDown(void) {
  proc_shm_detach_all(&parent_proc);
  proc_shm_detach_all(&child_proc);
  shm_set_allocator(NULL, NULL);
}

void test_ipc_rmid_releases_region_after_detach(void) {
  shm_region_t* region = shm_region_create(IPC_PRIVATE, PAGE_SIZE, 0600, NULL, NULL);
  TEST_ASSERT_NOT_NULL(region);
  int shmid = shm_region_id(region);

  virt_addr_t base = reserve_base(&parent_proc, PAGE_SIZE);
  TEST_ASSERT_TRUE(vm_map_shared(&parent_proc,
                                 region,
                                 base,
                                 VM_REGION_FLAG_USER | VM_REGION_FLAG_READ | VM_REGION_FLAG_WRITE,
                                 true));
  TEST_ASSERT_TRUE(proc_shm_track_attachment(&parent_proc, region, base, PAGE_SIZE, shmid, 0));

  shm_region_set_marked_for_removal(region, true);
  TEST_ASSERT_EQUAL_size_t(1, shm_region_attachment_count(region));

  TEST_ASSERT_TRUE(proc_shm_detach(&parent_proc, base));
  TEST_ASSERT_EQUAL_size_t(0, parent_proc.shm_attachment_count);

  shm_region_t* lookup = shm_region_get_by_id(shmid);
  TEST_ASSERT_NULL(lookup);
}

void test_proc_shm_inherit_and_cleanup(void) {
  shm_region_t* region = shm_region_create(IPC_PRIVATE, PAGE_SIZE, 0600, NULL, NULL);
  TEST_ASSERT_NOT_NULL(region);
  int shmid = shm_region_id(region);

  virt_addr_t base = reserve_base(&parent_proc, PAGE_SIZE);
  TEST_ASSERT_TRUE(vm_map_shared(&parent_proc,
                                 region,
                                 base,
                                 VM_REGION_FLAG_USER | VM_REGION_FLAG_READ | VM_REGION_FLAG_WRITE,
                                 true));
  TEST_ASSERT_TRUE(proc_shm_track_attachment(&parent_proc, region, base, PAGE_SIZE, shmid, 0));

  TEST_ASSERT_TRUE(proc_shm_inherit(&child_proc, &parent_proc));
  TEST_ASSERT_EQUAL_size_t(1, parent_proc.shm_attachment_count);
  TEST_ASSERT_EQUAL_size_t(1, child_proc.shm_attachment_count);

  virt_addr_t parent_base = parent_proc.shm_attachments[0].base;
  virt_addr_t child_base = child_proc.shm_attachments[0].base;

  shm_region_t* lookup = shm_region_get_by_id(shmid);
  TEST_ASSERT_NOT_NULL(lookup);
  TEST_ASSERT_EQUAL_size_t(2, shm_region_attachment_count(lookup));
  shm_region_unref(lookup);

  TEST_ASSERT_TRUE(proc_shm_detach(&child_proc, child_base));
  lookup = shm_region_get_by_id(shmid);
  TEST_ASSERT_NOT_NULL(lookup);
  TEST_ASSERT_EQUAL_size_t(1, shm_region_attachment_count(lookup));
  shm_region_unref(lookup);

  TEST_ASSERT_TRUE(proc_shm_detach(&parent_proc, parent_base));
  destroy_region(shmid);
}

void test_proc_shm_detach_all_releases_all_attachments(void) {
  shm_region_t* region = shm_region_create(IPC_PRIVATE, PAGE_SIZE, 0600, NULL, NULL);
  TEST_ASSERT_NOT_NULL(region);
  int shmid = shm_region_id(region);

  virt_addr_t base1 = reserve_base(&parent_proc, PAGE_SIZE);
  virt_addr_t base2 = reserve_base(&parent_proc, PAGE_SIZE);

  TEST_ASSERT_TRUE(vm_map_shared(&parent_proc,
                                 region,
                                 base1,
                                 VM_REGION_FLAG_USER | VM_REGION_FLAG_READ | VM_REGION_FLAG_WRITE,
                                 true));
  TEST_ASSERT_TRUE(proc_shm_track_attachment(&parent_proc, region, base1, PAGE_SIZE, shmid, 0));

  TEST_ASSERT_TRUE(vm_map_shared(&parent_proc,
                                 region,
                                 base2,
                                 VM_REGION_FLAG_USER | VM_REGION_FLAG_READ | VM_REGION_FLAG_WRITE,
                                 true));
  TEST_ASSERT_TRUE(proc_shm_track_attachment(&parent_proc, region, base2, PAGE_SIZE, shmid, 0));

  TEST_ASSERT_EQUAL_size_t(2, parent_proc.shm_attachment_count);
  shm_region_t* lookup = shm_region_get_by_id(shmid);
  TEST_ASSERT_NOT_NULL(lookup);
  TEST_ASSERT_EQUAL_size_t(2, shm_region_attachment_count(lookup));
  shm_region_unref(lookup);

  shm_region_set_marked_for_removal(region, true);
  proc_shm_detach_all(&parent_proc);
  TEST_ASSERT_EQUAL_size_t(0, parent_proc.shm_attachment_count);

  lookup = shm_region_get_by_id(shmid);
  TEST_ASSERT_NULL(lookup);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ipc_rmid_releases_region_after_detach);
  RUN_TEST(test_proc_shm_inherit_and_cleanup);
  RUN_TEST(test_proc_shm_detach_all_releases_all_attachments);
  return UNITY_END();
}
