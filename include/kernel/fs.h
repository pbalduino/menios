#ifndef MENIOS_INCLUDE_KERNEL_FS_H
#define MENIOS_INCLUDE_KERNEL_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/block_device.h>

typedef enum {
  FS_TYPE_UNKNOWN = 0,
  FS_TYPE_FAT32,
} fs_type_t;

typedef struct fs_mount_t fs_mount_t;

typedef struct fs_dir_entry_t {
  char     name[256];
  bool     is_directory;
  uint32_t size;
} fs_dir_entry_t;

typedef bool (*fs_dir_iter_t)(const fs_dir_entry_t* entry, void* context);

bool fs_mount_fat32_first(block_device_t* device, fs_mount_t** out_mount);
bool fs_mount_fat32_partition(block_device_t* device, uint32_t partition_index, fs_mount_t** out_mount);
void fs_unmount(fs_mount_t* mount);

fs_type_t fs_mount_type(const fs_mount_t* mount);

bool fs_list_directory(const fs_mount_t* mount, const char* path, fs_dir_iter_t iter, void* context);
bool fs_file_read(const fs_mount_t* mount, const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read);
bool fs_file_read_all(const fs_mount_t* mount, const char* path, void** out_buffer, size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif
