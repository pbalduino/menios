#include <kernel/fs.h>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <kernel/block_device.h>
#include <kernel/heap.h>
#include <kernel/serial.h>

#define GPT_HEADER_SIGNATURE 0x5452415020494645ull
#define FAT32_EOC_MARK        0x0FFFFFF8u
#define FAT32_BAD_CLUSTER     0x0FFFFFF7u

typedef struct __attribute__((packed)) gpt_header_t {
  uint64_t signature;
  uint32_t revision;
  uint32_t header_size;
  uint32_t header_crc32;
  uint32_t reserved;
  uint64_t current_lba;
  uint64_t backup_lba;
  uint64_t first_usable_lba;
  uint64_t last_usable_lba;
  uint8_t  disk_guid[16];
  uint64_t partition_entries_lba;
  uint32_t number_of_partition_entries;
  uint32_t size_of_partition_entry;
  uint32_t partition_array_crc32;
} gpt_header_t;

typedef struct __attribute__((packed)) gpt_entry_t {
  uint8_t  type_guid[16];
  uint8_t  unique_guid[16];
  uint64_t first_lba;
  uint64_t last_lba;
  uint64_t attributes;
  uint16_t name[36];
} gpt_entry_t;

typedef struct __attribute__((packed)) fat32_bpb_t {
  uint8_t  jmp_boot[3];
  char     oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t  sectors_per_cluster;
  uint16_t reserved_sector_count;
  uint8_t  num_fats;
  uint16_t root_entry_count;
  uint16_t total_sectors_16;
  uint8_t  media;
  uint16_t fat_size_16;
  uint16_t sectors_per_track;
  uint16_t num_heads;
  uint32_t hidden_sectors;
  uint32_t total_sectors_32;
  uint32_t fat_size_32;
  uint16_t ext_flags;
  uint16_t fs_version;
  uint32_t root_cluster;
  uint16_t fs_info_sector;
  uint16_t backup_boot_sector;
  uint8_t  reserved[12];
  uint8_t  drive_number;
  uint8_t  reserved1;
  uint8_t  boot_signature;
  uint32_t volume_id;
  char     volume_label[11];
  char     fs_type[8];
} fat32_bpb_t;

typedef struct __attribute__((packed)) fat32_dir_entry_raw_t {
  uint8_t  name[11];
  uint8_t  attr;
  uint8_t  nt_reserved;
  uint8_t  creation_time_tenths;
  uint16_t creation_time;
  uint16_t creation_date;
  uint16_t last_access_date;
  uint16_t first_cluster_high;
  uint16_t write_time;
  uint16_t write_date;
  uint16_t first_cluster_low;
  uint32_t file_size;
} fat32_dir_entry_raw_t;

typedef struct __attribute__((packed)) fat32_lfn_entry_t {
  uint8_t  order;
  uint16_t name1[5];
  uint8_t  attr;
  uint8_t  type;
  uint8_t  checksum;
  uint16_t name2[6];
  uint16_t first_cluster_low;
  uint16_t name3[2];
} fat32_lfn_entry_t;

typedef struct fat32_fs_t {
  block_device_t* device;
  uint64_t        partition_start_lba;
  uint64_t        partition_block_count;
  uint32_t        bytes_per_sector;
  uint32_t        sectors_per_cluster;
  uint32_t        reserved_sectors;
  uint32_t        num_fats;
  uint32_t        fat_size_sectors;
  uint32_t        root_cluster;
  uint64_t        fat_start_lba;
  uint64_t        data_start_lba;
  uint32_t        cluster_size_bytes;
  uint32_t        max_cluster_index;
  uint8_t*        fat_table;
  size_t          fat_table_bytes;
} fat32_fs_t;

struct fs_mount_t {
  fs_type_t type;
  fat32_fs_t fat32;
};

static inline char fat32_upcase(char ch) {
  if(ch >= 'a' && ch <= 'z') {
    return (char)(ch - 'a' + 'A');
  }
  return ch;
}

static bool fat32_equals_ignore_case(const char* a, const char* b) {
  if(a == NULL || b == NULL) {
    return false;
  }

  while(*a != '\0' && *b != '\0') {
    if(fat32_upcase(*a) != fat32_upcase(*b)) {
      return false;
    }
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static bool gpt_read_header(block_device_t* device, gpt_header_t* header, uint8_t* buffer) {
  if(!block_device_read(device, 1, buffer, 1)) {
    return false;
  }

  memcpy(header, buffer, sizeof(gpt_header_t));
  if(header->signature != GPT_HEADER_SIGNATURE) {
    return false;
  }

  if(header->header_size < sizeof(gpt_header_t)) {
    return false;
  }

  if(header->size_of_partition_entry == 0 || header->size_of_partition_entry > device->block_size) {
    return false;
  }

  return true;
}

static bool gpt_read_entry(block_device_t* device,
                           const gpt_header_t* header,
                           uint32_t index,
                           uint8_t* buffer,
                           gpt_entry_t* entry) {
  if(index >= header->number_of_partition_entries) {
    return false;
  }

  uint32_t entries_per_block = device->block_size / header->size_of_partition_entry;
  if(entries_per_block == 0) {
    return false;
  }

  uint64_t block_index = index / entries_per_block;
  uint64_t entry_lba = header->partition_entries_lba + block_index;
  uint32_t offset_in_block = index % entries_per_block;

  if(!block_device_read(device, entry_lba, buffer, 1)) {
    return false;
  }

  const uint8_t* src = buffer + (offset_in_block * header->size_of_partition_entry);
  memset(entry, 0, sizeof(gpt_entry_t));
  size_t copy_bytes = header->size_of_partition_entry < sizeof(gpt_entry_t)
                        ? header->size_of_partition_entry
                        : sizeof(gpt_entry_t);
  memcpy(entry, src, copy_bytes);
  return true;
}

static bool fat32_is_guid_zero(const uint8_t guid[16]) {
  for(size_t i = 0; i < 16; i++) {
    if(guid[i] != 0) {
      return false;
    }
  }
  return true;
}

static bool fat32_load_fat_table(fat32_fs_t* fs) {
  fs->fat_table_bytes = (size_t)fs->fat_size_sectors * fs->bytes_per_sector;
  fs->fat_table = kmalloc(fs->fat_table_bytes);
  if(fs->fat_table == NULL) {
    return false;
  }

  if(!block_device_read(fs->device, fs->fat_start_lba, fs->fat_table, fs->fat_size_sectors)) {
    kfree(fs->fat_table);
    fs->fat_table = NULL;
    fs->fat_table_bytes = 0;
    return false;
  }

  fs->max_cluster_index = (uint32_t)(fs->fat_table_bytes / sizeof(uint32_t));
  return true;
}

static bool fat32_validate_bpb(const fat32_bpb_t* bpb, block_device_t* device) {
  if(bpb->bytes_per_sector == 0 || bpb->sectors_per_cluster == 0) {
    return false;
  }

  if(bpb->bytes_per_sector != device->block_size) {
    return false;
  }

  if(bpb->num_fats == 0 || bpb->fat_size_32 == 0) {
    return false;
  }

  uint32_t total_sectors = bpb->total_sectors_32;
  if(total_sectors == 0) {
    total_sectors = bpb->total_sectors_16;
  }

  if(total_sectors == 0) {
    return false;
  }

  uint32_t data_sectors = total_sectors - (bpb->reserved_sector_count + (uint32_t)bpb->num_fats * bpb->fat_size_32);
  if(data_sectors == 0) {
    return false;
  }

  uint32_t total_clusters = data_sectors / bpb->sectors_per_cluster;
  if(total_clusters < 65525u) {
    return false;
  }

  return true;
}

static bool fat32_mount_from_partition(block_device_t* device,
                                       const gpt_entry_t* partition,
                                       uint8_t* sector_buffer,
                                       fs_mount_t** out_mount) {
  if(out_mount == NULL) {
    return false;
  }

  if(partition->first_lba == 0 || partition->last_lba <= partition->first_lba) {
    return false;
  }

  uint64_t partition_blocks = (partition->last_lba - partition->first_lba) + 1u;

  if(!block_device_read(device, partition->first_lba, sector_buffer, 1)) {
    return false;
  }

  if(sector_buffer[510] != 0x55 || sector_buffer[511] != 0xAA) {
    return false;
  }

  const fat32_bpb_t* bpb = (const fat32_bpb_t*)sector_buffer;
  if(!fat32_validate_bpb(bpb, device)) {
    return false;
  }

  fs_mount_t* mount = kmalloc(sizeof(fs_mount_t));
  if(mount == NULL) {
    return false;
  }
  memset(mount, 0, sizeof(fs_mount_t));

  mount->type = FS_TYPE_FAT32;
  fat32_fs_t* fs = &mount->fat32;
  fs->device = device;
  fs->partition_start_lba = partition->first_lba;
  fs->partition_block_count = partition_blocks;
  fs->bytes_per_sector = bpb->bytes_per_sector;
  fs->sectors_per_cluster = bpb->sectors_per_cluster;
  fs->reserved_sectors = bpb->reserved_sector_count;
  fs->num_fats = bpb->num_fats;
  fs->fat_size_sectors = bpb->fat_size_32;
  fs->root_cluster = bpb->root_cluster;
  fs->cluster_size_bytes = fs->bytes_per_sector * fs->sectors_per_cluster;
  fs->fat_start_lba = fs->partition_start_lba + fs->reserved_sectors;
  fs->data_start_lba = fs->fat_start_lba + (uint64_t)fs->num_fats * fs->fat_size_sectors;

  if(!fat32_load_fat_table(fs)) {
    kfree(mount);
    return false;
  }

  *out_mount = mount;
  serial_printf("fat32: mounted partition LBA %llu size %llu blocks\n",
                (unsigned long long)partition->first_lba,
                (unsigned long long)partition_blocks);
  return true;
}

static uint32_t fat32_get_fat_entry(const fat32_fs_t* fs, uint32_t cluster) {
  if(cluster >= fs->max_cluster_index) {
    return FAT32_BAD_CLUSTER;
  }
  const uint32_t* fat = (const uint32_t*)fs->fat_table;
  return fat[cluster] & 0x0FFFFFFFu;
}

static bool fat32_read_cluster(const fat32_fs_t* fs, uint32_t cluster, void* buffer) {
  if(cluster < 2u) {
    return false;
  }

  uint64_t lba = fs->data_start_lba + (uint64_t)(cluster - 2u) * fs->sectors_per_cluster;
  return block_device_read(fs->device, lba, buffer, fs->sectors_per_cluster);
}

typedef struct fat32_dir_entry_info_t {
  char     name[256];
  bool     is_directory;
  uint32_t first_cluster;
  uint32_t size;
} fat32_dir_entry_info_t;

static void fat32_reset_lfn(char* buffer, size_t length) {
  if(buffer && length > 0) {
    buffer[0] = '\0';
  }
}

static void fat32_decode_sfn(const uint8_t name[11], char* out, size_t out_capacity) {
  size_t pos = 0;
  size_t max_name = 8;
  while(pos < max_name && name[pos] != ' ') {
    if(pos + 1 < out_capacity) {
      out[pos] = (char)name[pos];
    }
    pos++;
  }

  size_t end = pos;
  if(name[8] != ' ') {
    if(end < out_capacity) {
      out[end++] = '.';
    }
    for(size_t i = 8; i < 11 && name[i] != ' '; i++) {
      if(end < out_capacity) {
        out[end++] = (char)name[i];
      }
    }
  }

  if(end < out_capacity) {
    out[end] = '\0';
  } else if(out_capacity > 0) {
    out[out_capacity - 1] = '\0';
  }
}

static void fat32_append_lfn_segment(char* target,
                                     size_t target_size,
                                     const fat32_lfn_entry_t* lfn) {
  if(target_size == 0) {
    return;
  }

  uint8_t order = lfn->order & 0x1Fu;
  if(order == 0) {
    return;
  }

  size_t base_index = (size_t)(order - 1u) * 13u;
  const uint16_t segments[13] = {
    lfn->name1[0], lfn->name1[1], lfn->name1[2], lfn->name1[3], lfn->name1[4],
    lfn->name2[0], lfn->name2[1], lfn->name2[2], lfn->name2[3], lfn->name2[4], lfn->name2[5],
    lfn->name3[0], lfn->name3[1]
  };

  for(size_t i = 0; i < 13; i++) {
    uint16_t code = segments[i];
    if(code == 0x0000 || code == 0xFFFF) {
      break;
    }
    size_t dst = base_index + i;
    if(dst + 1 < target_size) {
      target[dst] = (char)(code & 0xFFu);
      target[dst + 1] = '\0';
    }
  }
}

static uint32_t fat32_entry_first_cluster(const fat32_dir_entry_raw_t* raw) {
  uint32_t high = (uint32_t)raw->first_cluster_high << 16;
  uint32_t low = raw->first_cluster_low;
  return high | low;
}

static bool fat32_iterate_directory(const fat32_fs_t* fs,
                                    uint32_t start_cluster,
                                    fs_dir_iter_t iter,
                                    void* context,
                                    fat32_dir_entry_info_t* single_match,
                                    const char* match_name,
                                    bool match_directory_only) {
  uint8_t* cluster_buffer = kmalloc(fs->cluster_size_bytes);
  if(cluster_buffer == NULL) {
    return false;
  }

  uint32_t current_cluster = start_cluster;
  bool result = false;

  while(current_cluster >= 2u && current_cluster < fs->max_cluster_index) {
    if(!fat32_read_cluster(fs, current_cluster, cluster_buffer)) {
      goto cleanup;
    }

    const fat32_dir_entry_raw_t* entry = (const fat32_dir_entry_raw_t*)cluster_buffer;
    size_t entries_per_cluster = fs->cluster_size_bytes / sizeof(fat32_dir_entry_raw_t);
    char lfn[256];
    fat32_reset_lfn(lfn, sizeof(lfn));

    for(size_t idx = 0; idx < entries_per_cluster; idx++, entry++) {
      if(entry->name[0] == 0x00) {
        // End of directory entries
        result = true;
        goto cleanup;
      }

      if(entry->name[0] == 0xE5) {
        fat32_reset_lfn(lfn, sizeof(lfn));
        continue;
      }

      if(entry->attr == 0x0F) {
        const fat32_lfn_entry_t* lfn_entry = (const fat32_lfn_entry_t*)entry;
        if(lfn_entry->order & 0x40u) {
          fat32_reset_lfn(lfn, sizeof(lfn));
        }
        fat32_append_lfn_segment(lfn, sizeof(lfn), lfn_entry);
        continue;
      }

      if(entry->attr & 0x08u) {
        // Volume label
        fat32_reset_lfn(lfn, sizeof(lfn));
        continue;
      }

      fat32_dir_entry_info_t info;
      memset(&info, 0, sizeof(info));
      info.is_directory = (entry->attr & 0x10u) != 0;
      info.first_cluster = fat32_entry_first_cluster(entry);
      info.size = entry->file_size;

      if(info.first_cluster == 0 && info.is_directory) {
        info.first_cluster = fs->root_cluster;
      }

      if(lfn[0] != '\0') {
        strncpy(info.name, lfn, sizeof(info.name) - 1u);
        info.name[sizeof(info.name) - 1u] = '\0';
      } else {
        fat32_decode_sfn(entry->name, info.name, sizeof(info.name));
      }

      fat32_reset_lfn(lfn, sizeof(lfn));

      if(match_name != NULL) {
        if(match_directory_only && !info.is_directory) {
          continue;
        }
        if(fat32_equals_ignore_case(info.name, match_name)) {
          if(single_match) {
            *single_match = info;
          }
          result = true;
          goto cleanup;
        }
        continue;
      }

      if(iter) {
        fs_dir_entry_t public_entry;
        memset(&public_entry, 0, sizeof(public_entry));
        strncpy(public_entry.name, info.name, sizeof(public_entry.name) - 1u);
        public_entry.name[sizeof(public_entry.name) - 1u] = '\0';
        public_entry.is_directory = info.is_directory;
        public_entry.size = info.size;

        if(!iter(&public_entry, context)) {
          result = true;
          goto cleanup;
        }
      }
    }

    uint32_t next = fat32_get_fat_entry(fs, current_cluster);
    if(next >= FAT32_EOC_MARK) {
      break;
    }

    if(next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }

    current_cluster = next;
  }

  result = true;

cleanup:
  kfree(cluster_buffer);
  return result;
}

static bool fat32_find_entry(const fat32_fs_t* fs,
                             uint32_t start_cluster,
                             const char* name,
                             fat32_dir_entry_info_t* out_info,
                             bool require_directory) {
  if(out_info == NULL) {
    return false;
  }
  fat32_dir_entry_info_t tmp;
  if(!fat32_iterate_directory(fs, start_cluster, NULL, NULL, &tmp, name, require_directory)) {
    return false;
  }
  if(tmp.name[0] == '\0') {
    return false;
  }
  *out_info = tmp;
  return true;
}

static bool fat32_traverse_path(const fat32_fs_t* fs,
                                const char* path,
                                fat32_dir_entry_info_t* out_info,
                                bool final_must_be_directory) {
  if(path == NULL || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
    if(out_info) {
      memset(out_info, 0, sizeof(*out_info));
      out_info->is_directory = true;
      out_info->first_cluster = fs->root_cluster;
    }
    return true;
  }

  uint32_t current_cluster = fs->root_cluster;
  const char* iter = path;
  char component[256];

  if(*iter == '/') {
    iter++;
  }

  while(*iter != '\0') {
    size_t len = 0;
    while(iter[len] != '\0' && iter[len] != '/') {
      len++;
    }

    if(len == 0) {
      if(iter[len] == '/') {
        iter++;
        continue;
      }
      break;
    }

    size_t copy_len = len < (sizeof(component) - 1u) ? len : (sizeof(component) - 1u);
    for(size_t i = 0; i < copy_len; i++) {
      component[i] = iter[i];
    }
    component[copy_len] = '\0';

    fat32_dir_entry_info_t info;
    if(!fat32_find_entry(fs, current_cluster, component, &info, false)) {
      return false;
    }

    if(iter[len] == '\0') {
      if(final_must_be_directory && !info.is_directory) {
        return false;
      }
      if(out_info) {
        *out_info = info;
      }
      return true;
    }

    if(!info.is_directory) {
      return false;
    }

    current_cluster = info.first_cluster;
    iter += len;
    if(*iter == '/') {
      iter++;
    }
  }

  if(out_info) {
    out_info->is_directory = true;
    out_info->first_cluster = current_cluster;
  }
  return true;
}

static bool fat32_read_chain(const fat32_fs_t* fs,
                             uint32_t start_cluster,
                             size_t file_size,
                             size_t offset,
                             void* buffer,
                             size_t length,
                             size_t* bytes_read) {
  if(buffer == NULL || length == 0) {
    if(bytes_read) {
      *bytes_read = 0;
    }
    return true;
  }

  uint32_t cluster = start_cluster;
  size_t cluster_size = fs->cluster_size_bytes;
  size_t consumed = 0;
  size_t remaining = length;

  if(offset > file_size) {
    if(bytes_read) {
      *bytes_read = 0;
    }
    return true;
  }

  size_t file_remaining = file_size - offset;
  if(length > file_remaining) {
    remaining = file_remaining;
  }

  uint8_t* out = (uint8_t*)buffer;
  uint8_t* cluster_buffer = kmalloc(cluster_size);
  if(cluster_buffer == NULL) {
    return false;
  }

  size_t cluster_index = 0;

  while(cluster >= 2u && cluster < fs->max_cluster_index && remaining > 0) {
    if(!fat32_read_cluster(fs, cluster, cluster_buffer)) {
      kfree(cluster_buffer);
      return false;
    }

    size_t cluster_offset = cluster_index * cluster_size;
    size_t cluster_start = 0;

    if(offset >= cluster_offset + cluster_size) {
      // Entire cluster skipped
      cluster = fat32_get_fat_entry(fs, cluster);
      cluster_index++;
      continue;
    }

    if(offset > cluster_offset) {
      cluster_start = offset - cluster_offset;
    }

    size_t available_in_cluster = cluster_size - cluster_start;
    size_t to_copy = available_in_cluster < remaining ? available_in_cluster : remaining;
    memcpy(out, cluster_buffer + cluster_start, to_copy);
    out += to_copy;
    consumed += to_copy;
    remaining -= to_copy;

    uint32_t next = fat32_get_fat_entry(fs, cluster);
    if(next >= FAT32_EOC_MARK) {
      break;
    }
    if(next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    cluster = next;
    cluster_index++;
  }

  kfree(cluster_buffer);
  if(bytes_read) {
    *bytes_read = consumed;
  }
  return true;
}

bool fs_mount_fat32_partition(block_device_t* device,
                              uint32_t partition_index,
                              fs_mount_t** out_mount) {
  if(device == NULL || out_mount == NULL) {
    return false;
  }

  if(device->block_size == 0) {
    return false;
  }

  uint8_t* sector_buffer = kmalloc(device->block_size);
  if(sector_buffer == NULL) {
    return false;
  }

  gpt_header_t header;
  if(!gpt_read_header(device, &header, sector_buffer)) {
    kfree(sector_buffer);
    return false;
  }

  gpt_entry_t entry;
  if(!gpt_read_entry(device, &header, partition_index, sector_buffer, &entry)) {
    kfree(sector_buffer);
    return false;
  }

  if(fat32_is_guid_zero(entry.type_guid)) {
    kfree(sector_buffer);
    return false;
  }

  fs_mount_t* mount = NULL;
  bool ok = fat32_mount_from_partition(device, &entry, sector_buffer, &mount);
  kfree(sector_buffer);

  if(!ok) {
    return false;
  }

  *out_mount = mount;
  return true;
}

bool fs_mount_fat32_first(block_device_t* device, fs_mount_t** out_mount) {
  if(device == NULL || out_mount == NULL) {
    return false;
  }

  uint8_t* sector_buffer = kmalloc(device->block_size);
  if(sector_buffer == NULL) {
    return false;
  }

  gpt_header_t header;
  if(!gpt_read_header(device, &header, sector_buffer)) {
    kfree(sector_buffer);
    return false;
  }

  gpt_entry_t entry;
  for(uint32_t index = 0; index < header.number_of_partition_entries; index++) {
    if(!gpt_read_entry(device, &header, index, sector_buffer, &entry)) {
      continue;
    }
    if(fat32_is_guid_zero(entry.type_guid)) {
      continue;
    }

    fs_mount_t* mount = NULL;
    if(fat32_mount_from_partition(device, &entry, sector_buffer, &mount)) {
      kfree(sector_buffer);
      *out_mount = mount;
      return true;
    }
  }

  kfree(sector_buffer);
  return false;
}

fs_type_t fs_mount_type(const fs_mount_t* mount) {
  if(mount == NULL) {
    return FS_TYPE_UNKNOWN;
  }
  return mount->type;
}

void fs_unmount(fs_mount_t* mount) {
  if(mount == NULL) {
    return;
  }

  if(mount->type == FS_TYPE_FAT32) {
    fat32_fs_t* fs = &mount->fat32;
    if(fs->fat_table) {
      kfree(fs->fat_table);
      fs->fat_table = NULL;
      fs->fat_table_bytes = 0;
    }
  }

  kfree(mount);
}

bool fs_list_directory(const fs_mount_t* mount,
                       const char* path,
                       fs_dir_iter_t iter,
                       void* context) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32) {
    return false;
  }

  const fat32_fs_t* fs = &mount->fat32;
  fat32_dir_entry_info_t info;
  if(!fat32_traverse_path(fs, path, &info, true)) {
    return false;
  }

  uint32_t cluster = info.first_cluster ? info.first_cluster : fs->root_cluster;
  return fat32_iterate_directory(fs, cluster, iter, context, NULL, NULL, false);
}

bool fs_file_read(const fs_mount_t* mount,
                  const char* path,
                  size_t offset,
                  void* buffer,
                  size_t length,
                  size_t* bytes_read) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32 || buffer == NULL) {
    return false;
  }

  const fat32_fs_t* fs = &mount->fat32;
  fat32_dir_entry_info_t info;
  if(!fat32_traverse_path(fs, path, &info, false)) {
    return false;
  }

  if(info.is_directory) {
    return false;
  }

  return fat32_read_chain(fs, info.first_cluster, info.size, offset, buffer, length, bytes_read);
}

bool fs_file_read_all(const fs_mount_t* mount,
                      const char* path,
                      void** out_buffer,
                      size_t* out_size) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32 || out_buffer == NULL || out_size == NULL) {
    return false;
  }

  const fat32_fs_t* fs = &mount->fat32;
  fat32_dir_entry_info_t info;
  if(!fat32_traverse_path(fs, path, &info, false)) {
    return false;
  }

  if(info.is_directory) {
    return false;
  }

  void* buffer = kmalloc(info.size + 1u);
  if(buffer == NULL) {
    return false;
  }

  size_t bytes_read = 0;
  if(!fat32_read_chain(fs, info.first_cluster, info.size, 0, buffer, info.size, &bytes_read)) {
    kfree(buffer);
    return false;
  }

  if(bytes_read < info.size) {
    memset((uint8_t*)buffer + bytes_read, 0, info.size - bytes_read);
  }

  ((uint8_t*)buffer)[info.size] = '\0';
  *out_buffer = buffer;
  *out_size = info.size;
  return true;
}
