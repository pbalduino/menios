#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <kernel/fs.h>

#ifndef MENIOS_HOST_TEST
#error "stubs_fat32.c should only be compiled for host tests"
#endif

__attribute__((weak))
int fat32_mkdir_adapter(void* fs_ctx, const char* path, bool exclusive) {
  (void)fs_ctx;
  (void)path;
  (void)exclusive;
  return -ENOSYS;
}

__attribute__((weak))
int fat32_rmdir_adapter(void* fs_ctx, const char* path) {
  (void)fs_ctx;
  (void)path;
  return -ENOSYS;
}

__attribute__((weak))
int fat32_rename_adapter(void* fs_ctx, const char* old_path, const char* new_path) {
  (void)fs_ctx;
  (void)old_path;
  (void)new_path;
  return -ENOSYS;
}

__attribute__((weak))
bool fat32_datetime_to_timespec(uint16_t date,
                                uint16_t time,
                                uint8_t tenths,
                                bool has_time,
                                struct timespec* out_timespec) {
  (void)date;
  (void)time;
  (void)tenths;
  (void)has_time;
  if(out_timespec != NULL) {
    out_timespec->tv_sec = 0;
    out_timespec->tv_nsec = 0;
  }
  return false;
}
