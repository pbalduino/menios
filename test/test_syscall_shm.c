#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unity.h>

#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/shm.h>
#include <kernel/syscall.h>
#include <sys/shm.h>

static proc_info_t proc;
static proc_info_p old_current;

static phys_addr_t host_alloc_pages(size_t page_count) {
  size_t bytes = page_count * PAGE_SIZE;
  void* block = aligned_alloc(PAGE_SIZE, bytes);
  if(block == NULL) {
    return 0;
  }
  memset(block, 0, bytes);
  return (phys_addr_t)(uintptr_t)block;
}

static void host_free_pages(phys_addr_t base, size_t page_count) {
  (void)page_count;
  void* block = (void*)(uintptr_t)base;
  free(block);
}

void setUp(void) {
  shm_manager_init();
  shm_set_allocator(host_alloc_pages, host_free_pages);
  syscall_init();

  old_current = current;
  memset(&proc, 0, sizeof(proc));
  proc.pid = 1;
  proc.user_mode = true;
  proc.mmap_base = 0x100000000ULL;
  proc.mmap_limit = proc.mmap_base + 0x010000000ULL;
  proc.mmap_next = proc.mmap_base;

  current = &proc;
}

void tearDown(void) {
  proc_shm_detach_all(&proc);
  current = old_current;
  shm_set_allocator(NULL, NULL);
}

static void destroy_region(int shmid) {
  shm_region_t* region = shm_region_get_by_id(shmid);
  if(region == NULL) {
    return;
  }
  shm_region_set_marked_for_removal(region, true);
  shm_region_unref(region);
}

static uint64_t dispatch(uint64_t number, uint64_t rdi, uint64_t rsi, uint64_t rdx) {
  syscall_frame_t frame = {
    .rax = number,
    .rdi = rdi,
    .rsi = rsi,
    .rdx = rdx,
  };

  return syscall_dispatch(&frame);
}

void test_shmget_private_allocates_region(void) {
  uint64_t rc = dispatch(SYS_SHMGET, IPC_PRIVATE, PAGE_SIZE, 0600);
  TEST_ASSERT(rc > 0);

  int shmid = (int)rc;
  shm_region_t* region = shm_region_get_by_id(shmid);
  TEST_ASSERT_NOT_NULL(region);
  TEST_ASSERT_EQUAL_size_t(PAGE_SIZE, shm_region_size(region));
  TEST_ASSERT_EQUAL_size_t(1, shm_region_page_count(region));
  TEST_ASSERT_EQUAL_INT(IPC_PRIVATE, shm_region_key(region));
  shm_region_set_marked_for_removal(region, true);
  shm_region_unref(region);
}

void test_shmget_existing_key_returns_existing_id(void) {
  shm_key_t key = 0x42;
  uint64_t shmid = dispatch(SYS_SHMGET, (uint64_t)key, PAGE_SIZE, IPC_CREAT | 0600);
  TEST_ASSERT(shmid > 0);

  uint64_t second = dispatch(SYS_SHMGET, (uint64_t)key, PAGE_SIZE, 0600);
  TEST_ASSERT_EQUAL_UINT64(shmid, second);

  destroy_region((int)shmid);
}

void test_shmget_existing_key_with_exclusive_errors(void) {
  shm_key_t key = 0x99;
  uint64_t shmid = dispatch(SYS_SHMGET, (uint64_t)key, PAGE_SIZE, IPC_CREAT | 0600);
  TEST_ASSERT(shmid > 0);

  uint64_t rc = dispatch(SYS_SHMGET, (uint64_t)key, PAGE_SIZE, IPC_CREAT | IPC_EXCL | 0600);
  TEST_ASSERT_EQUAL_INT64(-EEXIST, (int64_t)rc);

  destroy_region((int)shmid);
}

void test_shmget_create_with_larger_size_errors(void) {
  shm_key_t key = 0x55;
  uint64_t shmid = dispatch(SYS_SHMGET, (uint64_t)key, PAGE_SIZE, IPC_CREAT | 0600);
  TEST_ASSERT(shmid > 0);

  uint64_t rc = dispatch(SYS_SHMGET, (uint64_t)key, PAGE_SIZE * 4, IPC_CREAT | 0600);
  TEST_ASSERT_EQUAL_INT64(-EINVAL, (int64_t)rc);

  destroy_region((int)shmid);
}

void test_shmat_attaches_and_updates_state(void) {
  uint64_t shmid = dispatch(SYS_SHMGET, IPC_PRIVATE, PAGE_SIZE, 0600);
  TEST_ASSERT(shmid > 0);

  uint64_t addr = dispatch(SYS_SHMAT, shmid, 0, 0);
  TEST_ASSERT_NOT_EQUAL((uint64_t)(-ENOMEM), addr);
  TEST_ASSERT_EQUAL_size_t(1, proc.shm_attachment_count);

  shm_region_t* region = shm_region_get_by_id((int)shmid);
  TEST_ASSERT_NOT_NULL(region);
  TEST_ASSERT_EQUAL_size_t(1, shm_region_attachment_count(region));

  uint64_t detach_rc = dispatch(SYS_SHMDT, addr, 0, 0);
  TEST_ASSERT_EQUAL_UINT64(0, detach_rc);
  TEST_ASSERT_EQUAL_size_t(0, proc.shm_attachment_count);

  shm_region_set_marked_for_removal(region, true);
  shm_region_unref(region);
}

void test_shmat_rejects_marked_for_removal_region(void) {
  uint64_t shmid = dispatch(SYS_SHMGET, IPC_PRIVATE, PAGE_SIZE, 0600);
  TEST_ASSERT(shmid > 0);

  uint64_t addr = dispatch(SYS_SHMAT, shmid, 0, 0);
  TEST_ASSERT_NOT_EQUAL((uint64_t)(-ENOMEM), addr);

  uint64_t ctl_rc = dispatch(SYS_SHMCTL, shmid, IPC_RMID, 0);
  TEST_ASSERT_EQUAL_UINT64(0, ctl_rc);

  uint64_t rc = dispatch(SYS_SHMAT, shmid, 0, 0);
  TEST_ASSERT_EQUAL_INT64(-EIDRM, (int64_t)rc);

  dispatch(SYS_SHMDT, addr, 0, 0);
}

void test_shmdt_detaches_and_updates_state(void) {
  uint64_t shmid = dispatch(SYS_SHMGET, IPC_PRIVATE, PAGE_SIZE, 0600);
  TEST_ASSERT(shmid > 0);

  uint64_t addr = dispatch(SYS_SHMAT, shmid, 0, 0);
  TEST_ASSERT_EQUAL_size_t(1, proc.shm_attachment_count);

  uint64_t rc = dispatch(SYS_SHMDT, addr, 0, 0);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_size_t(0, proc.shm_attachment_count);

  shm_region_t* region = shm_region_get_by_id((int)shmid);
  TEST_ASSERT_NOT_NULL(region);
  TEST_ASSERT_EQUAL_size_t(0, shm_region_attachment_count(region));
  shm_region_set_marked_for_removal(region, true);
  shm_region_unref(region);
}

void test_shmctl_ipc_stat_returns_metadata(void) {
  shm_key_t key = 0x5150;
  uint64_t shmid = dispatch(SYS_SHMGET, (uint64_t)key, PAGE_SIZE * 2, IPC_CREAT | 0644);
  TEST_ASSERT(shmid > 0);

  struct shmid_ds info;
  memset(&info, 0, sizeof(info));

  uint64_t rc = dispatch(SYS_SHMCTL, shmid, IPC_STAT, (uint64_t)&info);
  TEST_ASSERT_EQUAL_UINT64(0, rc);
  TEST_ASSERT_EQUAL_INT(key, info.shm_perm.key);
  TEST_ASSERT_EQUAL_UINT16(0644, info.shm_perm.mode);
  TEST_ASSERT_EQUAL_size_t(PAGE_SIZE * 2, info.shm_segsz);
  TEST_ASSERT_EQUAL_UINT16(0, info.shm_nattch);

  destroy_region((int)shmid);
}

void test_shmctl_ipc_rmid_marks_for_removal(void) {
  uint64_t shmid = dispatch(SYS_SHMGET, IPC_PRIVATE, PAGE_SIZE, 0600);
  TEST_ASSERT(shmid > 0);

  shm_region_t* region = shm_region_get_by_id((int)shmid);
  TEST_ASSERT_NOT_NULL(region);
  shm_region_unref(region);

  uint64_t rc = dispatch(SYS_SHMCTL, shmid, IPC_RMID, 0);
  TEST_ASSERT_EQUAL_UINT64(0, rc);

  region = shm_region_get_by_id((int)shmid);
  TEST_ASSERT_NULL(region);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_shmget_private_allocates_region);
  RUN_TEST(test_shmget_existing_key_returns_existing_id);
  RUN_TEST(test_shmget_existing_key_with_exclusive_errors);
  RUN_TEST(test_shmget_create_with_larger_size_errors);
  RUN_TEST(test_shmat_attaches_and_updates_state);
  RUN_TEST(test_shmat_rejects_marked_for_removal_region);
  RUN_TEST(test_shmdt_detaches_and_updates_state);
  RUN_TEST(test_shmctl_ipc_stat_returns_metadata);
  RUN_TEST(test_shmctl_ipc_rmid_marks_for_removal);

  return UNITY_END();
}
