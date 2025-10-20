#include <errno.h>
#include <stdbool.h>

#include <kernel/fs.h>

#ifndef MENIOS_HOST_TEST
#error "stubs_fat32.c should only be compiled for host tests"
#endif

int fat32_mkdir_adapter(void* fs_ctx, const char* path, bool exclusive) {
  (void)fs_ctx;
  (void)path;
  (void)exclusive;
  return -ENOSYS;
}

int fat32_rmdir_adapter(void* fs_ctx, const char* path) {
  (void)fs_ctx;
  (void)path;
  return -ENOSYS;
}

int fat32_rename_adapter(void* fs_ctx, const char* old_path, const char* new_path) {
  (void)fs_ctx;
  (void)old_path;
  (void)new_path;
  return -ENOSYS;
}
