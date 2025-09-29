#ifndef MENIOS_INCLUDE_KERNEL_VFS_H
#define MENIOS_INCLUDE_KERNEL_VFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/fs.h>
#include <kernel/file.h>
#include <kernel/block_device.h>

typedef fs_dir_entry_t vfs_dir_entry_t;
typedef fs_dir_iter_t  vfs_dir_iter_t;

typedef struct vfs_fs_driver_t {
  bool (*list)(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context);
  bool (*read)(void* fs_ctx, const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read);
  bool (*read_all)(void* fs_ctx, const char* path, void** out_buffer, size_t* out_size);
  void (*destroy)(void* fs_ctx);
} vfs_fs_driver_t;

bool vfs_init(void);
void vfs_shutdown(void);
bool vfs_mount(const char* path, const vfs_fs_driver_t* driver, void* fs_ctx);
bool vfs_mount_root(const vfs_fs_driver_t* driver, void* fs_ctx);
bool vfs_list(const char* path, vfs_dir_iter_t iter, void* context);
bool vfs_read(const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read);
bool vfs_read_all(const char* path, void** out_buffer, size_t* out_size);
file_t* vfs_open(const char* path);

bool vfs_mount_fat32_root(block_device_t* device);

#ifdef __cplusplus
}
#endif

#endif
