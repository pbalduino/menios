#ifndef MENIOS_INCLUDE_KERNEL_FS_H
#define MENIOS_INCLUDE_KERNEL_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sys/stat.h>
#include <time.h>

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

typedef struct fs_path_info_t {
  bool     is_directory;
  bool     is_read_only;
  uint32_t block_size;
  uint64_t inode;
  uint64_t size;
  bool     has_mode;
  mode_t   mode;
  bool     has_times;
  struct timespec atime;
  struct timespec mtime;
  struct timespec ctime;
  bool     has_dos_attributes;
  uint8_t  dos_attributes;
  bool     is_hidden;
  bool     is_system;
  bool     is_archived;
} fs_path_info_t;

static inline void fs_path_info_to_stat(const fs_path_info_t* info, struct stat* out_stat) {
  if(out_stat == NULL) {
    return;
  }

  memset(out_stat, 0, sizeof(*out_stat));

  if(info == NULL) {
    return;
  }

  if(info->has_mode) {
    out_stat->st_mode = info->mode;
  } else {
    mode_t mode = info->is_directory ? S_IFDIR : S_IFREG;
    mode_t perms;
    if(info->is_directory) {
      perms = info->is_read_only ? 0555 : 0755;
    } else {
      perms = info->is_read_only ? 0444 : 0644;
    }
    out_stat->st_mode = mode | perms;
  }
  out_stat->st_nlink = 1;
  out_stat->st_size = (off_t)info->size;
  out_stat->st_blksize = (blksize_t)(info->block_size ? info->block_size : 512);
  if(out_stat->st_blksize == 0) {
    out_stat->st_blksize = 512;
  }
  out_stat->st_blocks = (blkcnt_t)((info->size + 511ull) / 512ull);
  out_stat->st_ino = (ino_t)info->inode;
  out_stat->st_dev = 0;
  out_stat->st_rdev = 0;
  out_stat->st_uid = 0;
  out_stat->st_gid = 0;
  if(info->has_times) {
    out_stat->st_atime = (time_t)info->atime.tv_sec;
    out_stat->st_mtime = (time_t)info->mtime.tv_sec;
    out_stat->st_ctime = (time_t)info->ctime.tv_sec;
  } else {
    out_stat->st_atime = 0;
    out_stat->st_mtime = 0;
    out_stat->st_ctime = 0;
  }

  out_stat->st_menios_flags = 0;
  out_stat->st_menios_dos_attributes = 0;
  if(info->has_dos_attributes) {
    out_stat->st_menios_flags |= ST_MENIOS_FLAG_HAS_DOS_ATTRS;
    out_stat->st_menios_dos_attributes = info->dos_attributes;
    if(info->is_hidden) {
      out_stat->st_menios_flags |= ST_MENIOS_FLAG_DOS_HIDDEN;
    }
    if(info->is_system) {
      out_stat->st_menios_flags |= ST_MENIOS_FLAG_DOS_SYSTEM;
    }
    if(info->is_archived) {
      out_stat->st_menios_flags |= ST_MENIOS_FLAG_DOS_ARCHIVED;
    }
  }
}

bool fs_mount_fat32_first(block_device_t* device, fs_mount_t** out_mount);
bool fs_mount_fat32_partition(block_device_t* device, uint32_t partition_index, fs_mount_t** out_mount);
void fs_unmount(fs_mount_t* mount);

fs_type_t fs_mount_type(const fs_mount_t* mount);

bool fs_list_directory(const fs_mount_t* mount, const char* path, fs_dir_iter_t iter, void* context);
bool fs_file_read(const fs_mount_t* mount, const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read);
bool fs_file_read_all(const fs_mount_t* mount, const char* path, void** out_buffer, size_t* out_size);
bool fs_file_write(const fs_mount_t* mount, const char* path, size_t offset, const void* buffer, size_t length, size_t* bytes_written);
bool fs_file_write_all(const fs_mount_t* mount, const char* path, const void* buffer, size_t size);
bool fs_file_stat(const fs_mount_t* mount, const char* path, size_t* out_size);
bool fs_file_create(const fs_mount_t* mount, const char* path, bool exclusive);
bool fs_file_truncate(const fs_mount_t* mount, const char* path);
bool fs_directory_create(const fs_mount_t* mount, const char* path, bool exclusive);
bool fs_path_unlink(const fs_mount_t* mount, const char* path);
bool fs_directory_remove(const fs_mount_t* mount, const char* path);
bool fs_path_info(const fs_mount_t* mount, const char* path, fs_path_info_t* out_info);

#ifdef __cplusplus
}
#endif

#endif
