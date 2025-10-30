#ifndef MENIOS_INCLUDE_KERNEL_FS_VFS_H
#define MENIOS_INCLUDE_KERNEL_FS_VFS_H

#ifdef __cplusplus
extern "C" {
#endif /* MENIOS_INCLUDE_KERNEL_FS_VFS_H */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <kernel/fs/core.h>
#include <kernel/file.h>
#include <kernel/block_device.h>

#define VFS_PATH_MAX 256

typedef fs_dir_entry_t vfs_dir_entry_t;
typedef fs_dir_iter_t  vfs_dir_iter_t;

typedef struct vfs_fs_driver_t {
  bool (*list)(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context);
  bool (*read)(void* fs_ctx, const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read);
  bool (*read_all)(void* fs_ctx, const char* path, void** out_buffer, size_t* out_size);
  bool (*write)(void* fs_ctx, const char* path, size_t offset, const void* buffer, size_t length, size_t* bytes_written);
  bool (*write_all)(void* fs_ctx, const char* path, const void* buffer, size_t size);
  bool (*create_file)(void* fs_ctx, const char* path, bool exclusive);
  bool (*truncate_file)(void* fs_ctx, const char* path);
  bool (*stat)(void* fs_ctx, const char* path, fs_path_info_t* out_info);
  int (*open)(void* fs_ctx, const char* path, int flags, file_t** out_file);
  int (*unlink)(void* fs_ctx, const char* path);
  int (*mkdir)(void* fs_ctx, const char* path, bool exclusive);
  int (*rmdir)(void* fs_ctx, const char* path);
  int (*rename)(void* fs_ctx, const char* old_path, const char* new_path);
  int (*chmod)(void* fs_ctx, const char* path, mode_t mode);
  int (*utimens)(void* fs_ctx, const char* path, const struct timespec times[2]);
  void (*destroy)(void* fs_ctx);
} vfs_fs_driver_t;

bool vfs_init(void);
void vfs_shutdown(void);
bool vfs_mount(const char* path, const vfs_fs_driver_t* driver, void* fs_ctx, bool read_only);
bool vfs_mount_root(const vfs_fs_driver_t* driver, void* fs_ctx, bool read_only);
bool vfs_list(const char* path, vfs_dir_iter_t iter, void* context);
bool vfs_read(const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read);
bool vfs_read_all(const char* path, void** out_buffer, size_t* out_size);
int vfs_open(const char* path, int flags, file_t** out_file);
bool vfs_build_absolute_path(const char* base, const char* path, char* out, size_t out_size);
bool vfs_path_is_directory(const char* path);
bool vfs_path_info(const char* path, fs_path_info_t* out_info);
int vfs_unlink(const char* path);
int vfs_rmdir(const char* path);
int vfs_mkdir(const char* path);
int vfs_rename(const char* old_path, const char* new_path);
int vfs_chmod(const char* path, mode_t mode);
int vfs_utimens(const char* path, const struct timespec times[2]);

bool vfs_mount_fat32_root(block_device_t* device);

#ifdef __cplusplus
}
#endif

#endif
