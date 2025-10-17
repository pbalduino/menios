#include <kernel/fs.h>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/fcntl.h>

#include <kernel/block_device.h>
#include <kernel/heap.h>
#include <kernel/serial.h>
#include <kernel/file.h>
#include <kernel/file.h>

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

static void fat32_set_fat_entry(fat32_fs_t* fs, uint32_t cluster, uint32_t value) {
  if(cluster >= fs->max_cluster_index) {
    return;
  }
  uint32_t* fat = (uint32_t*)fs->fat_table;
  fat[cluster] = value & 0x0FFFFFFFu;
}

static uint32_t fat32_allocate_cluster(fat32_fs_t* fs) {
  if(fs->fat_table == NULL) {
    return 0;
  }

  uint32_t* fat = (uint32_t*)fs->fat_table;
  for(uint32_t cluster = 2u; cluster < fs->max_cluster_index; cluster++) {
    if((fat[cluster] & 0x0FFFFFFFu) == 0) {
      fat[cluster] = FAT32_EOC_MARK;
      return cluster;
    }
  }
  return 0;
}

static void fat32_free_cluster_chain(fat32_fs_t* fs, uint32_t start_cluster) {
  uint32_t cluster = start_cluster;
  while(cluster >= 2u && cluster < fs->max_cluster_index) {
    uint32_t next = fat32_get_fat_entry(fs, cluster);
    fat32_set_fat_entry(fs, cluster, 0);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    cluster = next;
  }
}

static bool fat32_flush_fat(fat32_fs_t* fs) {
  if(fs->fat_table == NULL) {
    return false;
  }

  for(uint32_t fat_index = 0; fat_index < fs->num_fats; fat_index++) {
    uint64_t lba = fs->fat_start_lba + (uint64_t)fat_index * fs->fat_size_sectors;
    if(!block_device_write(fs->device, lba, fs->fat_table, fs->fat_size_sectors)) {
      return false;
    }
  }
  return true;
}

static bool fat32_read_cluster(const fat32_fs_t* fs, uint32_t cluster, void* buffer) {
  if(cluster < 2u) {
    return false;
  }

  uint64_t lba = fs->data_start_lba + (uint64_t)(cluster - 2u) * fs->sectors_per_cluster;
  return block_device_read(fs->device, lba, buffer, fs->sectors_per_cluster);
}

static bool fat32_write_cluster(const fat32_fs_t* fs, uint32_t cluster, const void* buffer) {
  if(cluster < 2u) {
    return false;
  }

  uint64_t lba = fs->data_start_lba + (uint64_t)(cluster - 2u) * fs->sectors_per_cluster;
  return block_device_write(fs->device, lba, buffer, fs->sectors_per_cluster);
}

static bool fat32_zero_cluster(const fat32_fs_t* fs, uint32_t cluster) {
  uint8_t* zero = kmalloc(fs->cluster_size_bytes);
  if(zero == NULL) {
    return false;
  }
  memset(zero, 0, fs->cluster_size_bytes);
  bool ok = fat32_write_cluster(fs, cluster, zero);
  kfree(zero);
  return ok;
}

static size_t fat32_count_clusters(const fat32_fs_t* fs,
                                   uint32_t first_cluster,
                                   uint32_t* last_cluster_out) {
  size_t count = 0;
  uint32_t cluster = first_cluster;
  uint32_t last = 0;
  while(cluster >= 2u && cluster < fs->max_cluster_index) {
    last = cluster;
    count++;
    uint32_t next = fat32_get_fat_entry(fs, cluster);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    cluster = next;
  }
  if(last_cluster_out != NULL) {
    *last_cluster_out = (count == 0) ? 0 : last;
  }
  return count;
}

static bool fat32_directory_append_cluster(fat32_fs_t* fs,
                                           uint32_t dir_cluster,
                                           uint32_t* new_cluster_out) {
  uint32_t last = dir_cluster;
  while(true) {
    uint32_t next = fat32_get_fat_entry(fs, last);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    last = next;
  }

  uint32_t new_cluster = fat32_allocate_cluster(fs);
  if(new_cluster == 0) {
    return false;
  }

  fat32_set_fat_entry(fs, last, new_cluster);
  fat32_set_fat_entry(fs, new_cluster, FAT32_EOC_MARK);
  if(!fat32_zero_cluster(fs, new_cluster)) {
    fat32_set_fat_entry(fs, last, FAT32_EOC_MARK);
    fat32_set_fat_entry(fs, new_cluster, 0);
    return false;
  }

  if(new_cluster_out) {
    *new_cluster_out = new_cluster;
  }
  return true;
}

static bool fat32_directory_find_free_entries(fat32_fs_t* fs,
                                              uint32_t start_cluster,
                                              uint32_t required_entries,
                                              uint32_t* out_cluster,
                                              uint32_t* out_index,
                                              bool* used_end_marker) {
  if(out_cluster == NULL || out_index == NULL) {
    return false;
  }

  if(required_entries == 0) {
    required_entries = 1;
  }

  size_t entries_per_cluster = fs->cluster_size_bytes / sizeof(fat32_dir_entry_raw_t);
  uint8_t* buffer = kmalloc(fs->cluster_size_bytes);
  if(buffer == NULL) {
    return false;
  }

  uint32_t cluster = start_cluster;
  uint32_t run_start_cluster = 0;
  uint32_t run_start_index = 0;
  size_t run_length = 0;
  bool run_used_end = false;

  while(cluster >= 2u && cluster < fs->max_cluster_index) {
    if(!fat32_read_cluster(fs, cluster, buffer)) {
      kfree(buffer);
      return false;
    }

    fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;
    for(size_t idx = 0; idx < entries_per_cluster; idx++) {
      uint8_t first = entries[idx].name[0];
      bool free_entry = (first == 0xE5u) || (first == 0x00u);
      if(free_entry) {
        if(run_length == 0) {
          run_start_cluster = cluster;
          run_start_index = (uint32_t)idx;
          run_used_end = (first == 0x00u);
        } else if(first == 0x00u) {
          run_used_end = true;
        }

        run_length++;

        if(run_length >= required_entries) {
          *out_cluster = run_start_cluster;
          *out_index = run_start_index;
          if(used_end_marker) {
            *used_end_marker = run_used_end;
          }
          kfree(buffer);
          return true;
        }

        if(first == 0x00u) {
          size_t remaining_in_cluster = entries_per_cluster - idx - 1;
          if(run_length + remaining_in_cluster >= required_entries) {
            *out_cluster = run_start_cluster;
            *out_index = run_start_index;
            if(used_end_marker) {
              *used_end_marker = true;
            }
            kfree(buffer);
            return true;
          }
          // Need more entries beyond this cluster.
          kfree(buffer);
          goto append_clusters;
        }
      } else {
        run_length = 0;
        run_used_end = false;
      }
    }

    uint32_t next = fat32_get_fat_entry(fs, cluster);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    cluster = next;
  }

  kfree(buffer);

append_clusters:
  {
    uint32_t start_cluster_for_allocation = start_cluster;
    uint32_t starting_cluster = run_length > 0 ? run_start_cluster : 0;
    uint32_t starting_index = run_length > 0 ? run_start_index : 0;
    bool starting_used_end = run_used_end;

    size_t remaining_needed = (run_length >= required_entries) ? 0
                               : (required_entries - run_length);
    size_t clusters_needed = (remaining_needed + entries_per_cluster - 1u) / entries_per_cluster;
    if(run_length == 0 && clusters_needed == 0) {
      clusters_needed = (required_entries + entries_per_cluster - 1u) / entries_per_cluster;
    }

    uint32_t first_new_cluster = 0;
    for(size_t i = 0; i < clusters_needed; i++) {
      uint32_t new_cluster = 0;
      if(!fat32_directory_append_cluster(fs, start_cluster_for_allocation, &new_cluster)) {
        return false;
      }
      if(first_new_cluster == 0) {
        first_new_cluster = new_cluster;
      }
    }

    if(run_length > 0) {
      *out_cluster = run_start_cluster;
      *out_index = run_start_index;
      if(used_end_marker) {
        *used_end_marker = run_used_end || clusters_needed > 0;
      }
      return true;
    }

    if(first_new_cluster == 0) {
      if(!fat32_directory_append_cluster(fs, start_cluster_for_allocation, &first_new_cluster)) {
        return false;
      }
    }

    *out_cluster = first_new_cluster;
    *out_index = 0;
    if(used_end_marker) {
      *used_end_marker = true;
    }
    return true;
  }
}

static const char* fat32_strrchr(const char* str, char ch) {
  const char* last = NULL;
  while(str && *str) {
    if(*str == ch) {
      last = str;
    }
    str++;
  }
  return last;
}

static bool fat32_split_path(const char* path,
                             char* parent,
                             size_t parent_capacity,
                             char* name,
                             size_t name_capacity) {
  if(path == NULL || parent == NULL || name == NULL) {
    return false;
  }

  char relative[256];
  size_t path_len = strlen(path);
  if(path_len == 0) {
    return false;
  }

  if(path[0] != '/') {
    if(path_len + 2u > sizeof(relative)) {
      return false;
    }
    relative[0] = '/';
    strcpy(relative + 1, path);
  } else {
    if(path_len + 1u > sizeof(relative)) {
      return false;
    }
    strcpy(relative, path);
  }

  const char* last_slash = fat32_strrchr(relative, '/');
  if(last_slash == NULL) {
    return false;
  }

  size_t parent_len = (size_t)(last_slash - relative);
  if(parent_len == 0) {
    if(parent_capacity < 2u) {
      return false;
    }
    strcpy(parent, "/");
  } else {
    if(parent_len + 1u > parent_capacity) {
      return false;
    }
    memcpy(parent, relative, parent_len);
    parent[parent_len] = '\0';
  }

  const char* child = last_slash + 1;
  if(*child == '\0') {
    return false;
  }
  size_t child_len = strlen(child);
  if(child_len + 1u > name_capacity) {
    return false;
  }
  strcpy(name, child);
  return true;
}

static bool fat32_is_valid_sfn_char(char ch) {
  if(ch >= 'A' && ch <= 'Z') {
    return true;
  }
  if(ch >= '0' && ch <= '9') {
    return true;
  }
  switch(ch) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '(': 
    case ')':
    case '-':
    case '@':
    case '^':
    case '_':
    case '`':
    case '{':
    case '}':
    case '~':
      return true;
    default:
      return false;
  }
}

static char fat32_normalize_sfn_char(char ch, bool* replaced) {
  if(ch >= 'a' && ch <= 'z') {
    ch = (char)(ch - 'a' + 'A');
  }
  if(!fat32_is_valid_sfn_char(ch)) {
    if(replaced) {
      *replaced = true;
    }
    return '_';
  }
  return ch;
}

static bool fat32_directory_sfn_exists(const fat32_fs_t* fs,
                                       uint32_t start_cluster,
                                       const uint8_t sfn[11]) {
  uint8_t* buffer = kmalloc(fs->cluster_size_bytes);
  if(buffer == NULL) {
    return true;
  }

  uint32_t cluster = start_cluster;
  while(cluster >= 2u && cluster < fs->max_cluster_index) {
    if(!fat32_read_cluster(fs, cluster, buffer)) {
      kfree(buffer);
      return true;
    }

    const fat32_dir_entry_raw_t* entry = (const fat32_dir_entry_raw_t*)buffer;
    size_t entries_per_cluster = fs->cluster_size_bytes / sizeof(fat32_dir_entry_raw_t);

    for(size_t idx = 0; idx < entries_per_cluster; idx++, entry++) {
      if(entry->name[0] == 0x00u) {
        kfree(buffer);
        return false;
      }
      if(entry->name[0] == 0xE5u) {
        continue;
      }
      if(entry->attr == 0x0Fu) {
        continue;
      }
      if(memcmp(entry->name, sfn, 11) == 0) {
        kfree(buffer);
        return true;
      }
    }

    uint32_t next = fat32_get_fat_entry(fs, cluster);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    cluster = next;
  }

  kfree(buffer);
  return false;
}

static size_t fat32_u32_to_string(uint32_t value, char* buffer, size_t capacity) {
  if(buffer == NULL || capacity == 0) {
    return 0;
  }

  char tmp[10];
  size_t len = 0;
  do {
    tmp[len++] = (char)('0' + (value % 10u));
    value /= 10u;
  } while(value != 0u && len < sizeof(tmp));

  if(len >= capacity) {
    len = capacity - 1u;
  }

  for(size_t i = 0; i < len; i++) {
    buffer[i] = tmp[len - 1u - i];
  }
  buffer[len] = '\0';
  return len;
}

static bool fat32_generate_sfn(fat32_fs_t* fs,
                               uint32_t dir_cluster,
                               const char* name,
                               uint8_t out[11],
                               bool* out_requires_lfn) {
  if(fs == NULL || name == NULL || name[0] == '\0' || out == NULL) {
    return false;
  }

  const char* dot = fat32_strrchr(name, '.');
  size_t name_len = strlen(name);
  size_t base_len = name_len;
  size_t ext_len = 0;

  if(dot != NULL && dot != name) {
    base_len = (size_t)(dot - name);
    if(dot[1] != '\0') {
      ext_len = strlen(dot + 1);
    }
  }

  bool replaced = false;
  bool base_truncated = false;
  bool ext_truncated = false;
  bool lower_seen = false;

  char sanitized_base_full[8];
  size_t sanitized_base_len = 0;
  for(size_t i = 0; i < base_len; i++) {
    char ch = name[i];
    if(ch >= 'a' && ch <= 'z') {
      lower_seen = true;
    }
    if(ch == ' ' || ch == '.') {
      replaced = true;
      ch = '_';
    }
    char normalized = fat32_normalize_sfn_char(ch, &replaced);
    if(sanitized_base_len < sizeof(sanitized_base_full)) {
      sanitized_base_full[sanitized_base_len++] = normalized;
    } else {
      base_truncated = true;
    }
  }

  if(sanitized_base_len == 0) {
    sanitized_base_full[0] = '_';
    sanitized_base_len = 1;
    replaced = true;
  }

  char sanitized_ext[3];
  size_t sanitized_ext_len = 0;
  for(size_t i = 0; i < ext_len; i++) {
    char ch = dot[1 + i];
    if(ch >= 'a' && ch <= 'z') {
      lower_seen = true;
    }
    if(ch == ' ') {
      replaced = true;
      ch = '_';
    }
    char normalized = fat32_normalize_sfn_char(ch, &replaced);
    if(sanitized_ext_len < sizeof(sanitized_ext)) {
      sanitized_ext[sanitized_ext_len++] = normalized;
    } else {
      ext_truncated = true;
    }
  }

  memset(out, ' ', 11);
  size_t copy_base = sanitized_base_len < 8 ? sanitized_base_len : 8;
  for(size_t i = 0; i < copy_base; i++) {
    out[i] = sanitized_base_full[i];
  }
  if(sanitized_base_len > 8) {
    base_truncated = true;
  }
  for(size_t i = 0; i < sanitized_ext_len && i < 3; i++) {
    out[8 + i] = sanitized_ext[i];
  }
  if(sanitized_ext_len > 3) {
    ext_truncated = true;
  }

  bool requires_lfn = base_truncated || ext_truncated || replaced;
  if(lower_seen) {
    requires_lfn = true;
  }

  uint8_t candidate[11];
  for(uint32_t attempt = 0; attempt < 1000000u; attempt++) {
    memcpy(candidate, out, sizeof(candidate));
    if(attempt > 0) {
      char digits[8];
      size_t digits_len = fat32_u32_to_string(attempt, digits, sizeof(digits));
      if(digits_len >= 7) {
        return false;
      }
      size_t keep = (sanitized_base_len < 8) ? sanitized_base_len : 8;
      if(keep > (8 - (digits_len + 1))) {
        keep = 8 - (digits_len + 1);
      }
      if(keep == 0) {
        keep = 1;
      }
      for(size_t i = 0; i < 8; i++) {
        candidate[i] = ' ';
      }
      for(size_t i = 0; i < keep; i++) {
        candidate[i] = sanitized_base_full[i];
      }
      candidate[keep] = '~';
      for(size_t i = 0; i < digits_len && (keep + 1 + i) < 8; i++) {
        candidate[keep + 1 + i] = digits[i];
      }
    }

    if(!fat32_directory_sfn_exists(fs, dir_cluster, candidate)) {
      memcpy(out, candidate, sizeof(candidate));
      if(out_requires_lfn) {
        *out_requires_lfn = requires_lfn || (attempt > 0);
      }
      return true;
    }
  }

  return false;
}

static uint8_t fat32_compute_sfn_checksum(const uint8_t sfn[11]) {
  uint8_t sum = 0;
  for(size_t i = 0; i < 11; i++) {
    sum = ((sum & 1u) ? 0x80u : 0u) + (sum >> 1) + sfn[i];
  }
  return sum;
}

static void fat32_fill_lfn_entry(fat32_lfn_entry_t* entry,
                                 uint8_t order,
                                 uint8_t checksum,
                                 const char* name,
                                 size_t name_len,
                                 size_t chunk_index) {
  memset(entry, 0xFF, sizeof(*entry));
  entry->order = order;
  entry->attr = 0x0Fu;
  entry->type = 0;
  entry->checksum = checksum;
  entry->first_cluster_low = 0;

  size_t start = chunk_index * 13u;
  for(size_t i = 0; i < 13; i++) {
    size_t pos = start + i;
    uint16_t code;
    if(pos < name_len) {
      code = (uint16_t)(uint8_t)name[pos];
    } else if(pos == name_len) {
      code = 0x0000u;
    } else {
      code = 0xFFFFu;
    }

    if(i < 5) {
      entry->name1[i] = code;
    } else if(i < 11) {
      entry->name2[i - 5] = code;
    } else {
      entry->name3[i - 11] = code;
    }
  }
}

static bool fat32_directory_write_entries(fat32_fs_t* fs,
                                          uint32_t start_cluster,
                                          uint32_t start_index,
                                          const uint8_t* raw_entries,
                                          size_t entry_count,
                                          bool used_end_marker) {
  if(entry_count == 0) {
    return true;
  }

  size_t entries_per_cluster = fs->cluster_size_bytes / sizeof(fat32_dir_entry_raw_t);
  uint8_t* buffer = kmalloc(fs->cluster_size_bytes);
  if(buffer == NULL) {
    return false;
  }

  uint32_t cluster = start_cluster;
  uint32_t index = start_index;
  size_t written = 0;

  while(written < entry_count && cluster >= 2u && cluster < fs->max_cluster_index) {
    if(!fat32_read_cluster(fs, cluster, buffer)) {
      kfree(buffer);
      return false;
    }

    fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;
    bool dirty = false;

    while(written < entry_count && index < entries_per_cluster) {
      const uint8_t* src = raw_entries + written * sizeof(fat32_dir_entry_raw_t);
      memcpy(&entries[index], src, sizeof(fat32_dir_entry_raw_t));
      written++;
      index++;
      dirty = true;
    }

    if(dirty) {
      if(!fat32_write_cluster(fs, cluster, buffer)) {
        kfree(buffer);
        return false;
      }
    }

    if(written == entry_count) {
      break;
    }

    uint32_t next = fat32_get_fat_entry(fs, cluster);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      kfree(buffer);
      return false;
    }
    cluster = next;
    index = 0;
  }

  if(used_end_marker) {
    uint32_t sentinel_cluster = cluster;
    uint32_t sentinel_index = index;

    while(sentinel_index >= entries_per_cluster) {
      uint32_t next = fat32_get_fat_entry(fs, sentinel_cluster);
      if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
        kfree(buffer);
        return true;
      }
      sentinel_cluster = next;
      sentinel_index -= (uint32_t)entries_per_cluster;
    }

    if(!fat32_read_cluster(fs, sentinel_cluster, buffer)) {
      kfree(buffer);
      return false;
    }
    fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;
    memset(&entries[sentinel_index], 0, sizeof(fat32_dir_entry_raw_t));
    if(!fat32_write_cluster(fs, sentinel_cluster, buffer)) {
      kfree(buffer);
      return false;
    }
  }

  kfree(buffer);
  return true;
}

static bool fat32_setup_directory_cluster(fat32_fs_t* fs,
                                          uint32_t cluster,
                                          uint32_t parent_cluster) {
  uint8_t* buffer = kmalloc(fs->cluster_size_bytes);
  if(buffer == NULL) {
    return false;
  }
  memset(buffer, 0, fs->cluster_size_bytes);

  fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;
  memset(&entries[0], 0, sizeof(fat32_dir_entry_raw_t));
  memcpy(entries[0].name, ".          ", 11);
  entries[0].attr = 0x10u;
  entries[0].first_cluster_low = (uint16_t)(cluster & 0xFFFFu);
  entries[0].first_cluster_high = (uint16_t)((cluster >> 16) & 0xFFFFu);

  memset(&entries[1], 0, sizeof(fat32_dir_entry_raw_t));
  memcpy(entries[1].name, "..         ", 11);
  entries[1].attr = 0x10u;
  uint32_t parent = parent_cluster;
  if(parent < 2u) {
    parent = 0;
  }
  entries[1].first_cluster_low = (uint16_t)(parent & 0xFFFFu);
  entries[1].first_cluster_high = (uint16_t)((parent >> 16) & 0xFFFFu);

  bool ok = fat32_write_cluster(fs, cluster, buffer);
  kfree(buffer);
  return ok;
}

typedef struct fat32_dir_entry_info_t {
  char                    name[256];
  bool                    is_directory;
  uint32_t                first_cluster;
  uint32_t                size;
  uint32_t                dir_cluster;
  uint32_t                dir_entry_index;
  uint8_t                 lfn_entries;
  fat32_dir_entry_raw_t   raw_entry;
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
    size_t dst = base_index + i;

    if(dst >= target_size) {
      target[target_size - 1] = '\0';
      return;
    }

    if(code == 0xFFFFu) {
      continue;
    }

    if(code == 0x0000u) {
      target[dst] = '\0';
      return;
    }

    target[dst] = (char)(code & 0xFFu);
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
    size_t lfn_count = 0;

    for(size_t idx = 0; idx < entries_per_cluster; idx++, entry++) {
      if(entry->name[0] == 0x00) {
        // End of directory entries
        result = true;
        goto cleanup;
      }

      if(entry->name[0] == 0xE5) {
        fat32_reset_lfn(lfn, sizeof(lfn));
        lfn_count = 0;
        continue;
      }

      if(entry->attr == 0x0F) {
        const fat32_lfn_entry_t* lfn_entry = (const fat32_lfn_entry_t*)entry;
        if(lfn_entry->order & 0x40u) {
          fat32_reset_lfn(lfn, sizeof(lfn));
          lfn_count = 0;
        }
        fat32_append_lfn_segment(lfn, sizeof(lfn), lfn_entry);
        lfn_count++;
        continue;
      }

      if(entry->attr & 0x08u) {
        // Volume label
        fat32_reset_lfn(lfn, sizeof(lfn));
        lfn_count = 0;
        continue;
      }

      fat32_dir_entry_info_t info;
      memset(&info, 0, sizeof(info));
      info.dir_cluster = current_cluster;
      info.dir_entry_index = (uint32_t)idx;
      memcpy(&info.raw_entry, entry, sizeof(fat32_dir_entry_raw_t));
      info.is_directory = (entry->attr & 0x10u) != 0;
      info.first_cluster = fat32_entry_first_cluster(entry);
      info.size = entry->file_size;
      info.lfn_entries = (uint8_t)lfn_count;

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
      lfn_count = 0;

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
    out_info->dir_cluster = current_cluster;
    out_info->dir_entry_index = 0;
    memset(&out_info->raw_entry, 0, sizeof(out_info->raw_entry));
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

static bool fat32_write_chain(fat32_fs_t* fs,
                              fat32_dir_entry_info_t* info,
                              size_t previous_size,
                              size_t offset,
                              const void* buffer,
                              size_t length,
                              size_t* bytes_written) {
  if(buffer == NULL || length == 0) {
    if(bytes_written) {
      *bytes_written = 0;
    }
    return true;
  }

  if(info->first_cluster == 0 || offset + length > info->size) {
    return false;
  }

  uint8_t* cluster_buffer = kmalloc(fs->cluster_size_bytes);
  if(cluster_buffer == NULL) {
    return false;
  }

  const uint8_t* in = (const uint8_t*)buffer;
  size_t written = 0;
  uint32_t cluster = info->first_cluster;
  size_t cluster_index = 0;

  while(cluster >= 2u && cluster < fs->max_cluster_index && written < length) {
    if(!fat32_read_cluster(fs, cluster, cluster_buffer)) {
      kfree(cluster_buffer);
      return false;
    }

    size_t cluster_offset = cluster_index * fs->cluster_size_bytes;
    if(offset >= cluster_offset + fs->cluster_size_bytes) {
      uint32_t next = fat32_get_fat_entry(fs, cluster);
      if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
        break;
      }
      cluster = next;
      cluster_index++;
      continue;
    }

    size_t start = 0;
    if(offset > cluster_offset) {
      start = offset - cluster_offset;
    }

    size_t valid_bytes = 0;
    if(previous_size > cluster_offset) {
      valid_bytes = previous_size - cluster_offset;
      if(valid_bytes > fs->cluster_size_bytes) {
        valid_bytes = fs->cluster_size_bytes;
      }
    }
    if(valid_bytes < start) {
      memset(cluster_buffer + valid_bytes, 0, start - valid_bytes);
    }

    size_t available = fs->cluster_size_bytes - start;
    size_t remaining = length - written;
    size_t to_copy = (available < remaining) ? available : remaining;
    memcpy(cluster_buffer + start, in + written, to_copy);

    if(!fat32_write_cluster(fs, cluster, cluster_buffer)) {
      kfree(cluster_buffer);
      return false;
    }

    written += to_copy;

    uint32_t next = fat32_get_fat_entry(fs, cluster);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    cluster = next;
    cluster_index++;
  }

  kfree(cluster_buffer);
  if(bytes_written) {
    *bytes_written = written;
  }
  return written == length;
}

static bool fat32_update_dir_entry(fat32_fs_t* fs,
                                   const fat32_dir_entry_info_t* info,
                                   uint32_t new_first_cluster,
                                   uint32_t new_size) {
  if(info->dir_cluster < 2u || info->dir_entry_index >= fs->cluster_size_bytes / sizeof(fat32_dir_entry_raw_t)) {
    return false;
  }

  uint8_t* buffer = kmalloc(fs->cluster_size_bytes);
  if(buffer == NULL) {
    return false;
  }

  if(!fat32_read_cluster(fs, info->dir_cluster, buffer)) {
    kfree(buffer);
    return false;
  }

  fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;
  fat32_dir_entry_raw_t* entry = &entries[info->dir_entry_index];

  if(new_first_cluster != UINT32_MAX) {
    entry->first_cluster_low = (uint16_t)(new_first_cluster & 0xFFFFu);
    entry->first_cluster_high = (uint16_t)((new_first_cluster >> 16) & 0xFFFFu);
  }

  if(new_size != UINT32_MAX) {
    entry->file_size = new_size;
  }

  bool ok = fat32_write_cluster(fs, info->dir_cluster, buffer);
  kfree(buffer);
  return ok;
}

static bool fat32_adjust_file_size(fat32_fs_t* fs,
                                   fat32_dir_entry_info_t* info,
                                   size_t new_size,
                                   bool* fat_dirty) {
  size_t old_size = info->size;
  size_t cluster_size = fs->cluster_size_bytes;
  size_t current_clusters = (old_size == 0) ? 0 : ((old_size + cluster_size - 1) / cluster_size);
  size_t required_clusters = (new_size == 0) ? 0 : ((new_size + cluster_size - 1) / cluster_size);

  if(required_clusters == current_clusters) {
    info->size = (uint32_t)new_size;
    if(new_size == 0) {
      info->first_cluster = 0;
    }
    return true;
  }

  if(required_clusters > current_clusters) {
    uint32_t last_cluster = 0;
    if(current_clusters > 0) {
      fat32_count_clusters(fs, info->first_cluster, &last_cluster);
    }

    uint32_t first_new = 0;
    for(size_t i = 0; i < (required_clusters - current_clusters); i++) {
      uint32_t new_cluster = fat32_allocate_cluster(fs);
      if(new_cluster == 0) {
        if(first_new != 0) {
          fat32_free_cluster_chain(fs, first_new);
          if(last_cluster != 0) {
            fat32_set_fat_entry(fs, last_cluster, FAT32_EOC_MARK);
          }
        }
        return false;
      }

      if(first_new == 0) {
        first_new = new_cluster;
      }

      if(last_cluster != 0) {
        fat32_set_fat_entry(fs, last_cluster, new_cluster);
      } else {
        info->first_cluster = new_cluster;
      }
      last_cluster = new_cluster;
      fat32_set_fat_entry(fs, new_cluster, FAT32_EOC_MARK);
      if(!fat32_zero_cluster(fs, new_cluster)) {
        fat32_free_cluster_chain(fs, new_cluster);
        if(first_new != new_cluster) {
          fat32_free_cluster_chain(fs, first_new);
        }
        return false;
      }
    }

    if(fat_dirty) {
      *fat_dirty = true;
    }
  } else {
    if(required_clusters == 0) {
      if(info->first_cluster != 0) {
        fat32_free_cluster_chain(fs, info->first_cluster);
        info->first_cluster = 0;
        if(fat_dirty) {
          *fat_dirty = true;
        }
      }
    } else {
      uint32_t last_to_keep = info->first_cluster;
      for(size_t i = 1; i < required_clusters; i++) {
        uint32_t next = fat32_get_fat_entry(fs, last_to_keep);
        if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
          break;
        }
        last_to_keep = next;
      }

      uint32_t next = fat32_get_fat_entry(fs, last_to_keep);
      if(next < FAT32_EOC_MARK && next != FAT32_BAD_CLUSTER && next >= 2u) {
        fat32_free_cluster_chain(fs, next);
      }
      fat32_set_fat_entry(fs, last_to_keep, FAT32_EOC_MARK);
      if(fat_dirty) {
        *fat_dirty = true;
      }
    }
  }

  info->size = (uint32_t)new_size;
  if(new_size == 0) {
    info->first_cluster = 0;
  }
  return true;
}

static bool fat32_create_entry(fat32_fs_t* fs,
                               const char* path,
                               bool exclusive,
                               bool create_directory,
                               fat32_dir_entry_info_t* out_info) {
  if(fs == NULL || path == NULL) {
    return false;
  }

  char parent[256];
  char name[256];
  if(!fat32_split_path(path, parent, sizeof(parent), name, sizeof(name))) {
    return false;
  }

  if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    return false;
  }

  fat32_dir_entry_info_t dir_info;
  if(!fat32_traverse_path(fs, parent, &dir_info, true)) {
    return false;
  }

  fat32_dir_entry_info_t existing;
  if(fat32_find_entry(fs, dir_info.first_cluster, name, &existing, false)) {
    if(exclusive) {
      return false;
    }
    if(existing.is_directory != create_directory) {
      return false;
    }
    if(out_info) {
      *out_info = existing;
    }
    return true;
  }

  uint8_t sfn[11];
  bool requires_lfn = false;
  if(!fat32_generate_sfn(fs, dir_info.first_cluster, name, sfn, &requires_lfn)) {
    return false;
  }

  size_t name_len = strlen(name);
  size_t lfn_entries = requires_lfn ? ((name_len + 12u) / 13u) : 0u;
  size_t total_entries = lfn_entries + 1u;

  uint32_t entry_cluster = 0;
  uint32_t entry_index = 0;
  bool used_end_marker = false;
  if(!fat32_directory_find_free_entries(fs,
                                        dir_info.first_cluster,
                                        (uint32_t)total_entries,
                                        &entry_cluster,
                                        &entry_index,
                                        &used_end_marker)) {
    return false;
  }

  uint8_t* entry_bytes = kmalloc(total_entries * sizeof(fat32_dir_entry_raw_t));
  if(entry_bytes == NULL) {
    return false;
  }

  uint32_t new_dir_cluster = 0;
  if(create_directory) {
    new_dir_cluster = fat32_allocate_cluster(fs);
    if(new_dir_cluster == 0) {
      kfree(entry_bytes);
      return false;
    }
    if(!fat32_setup_directory_cluster(fs, new_dir_cluster, dir_info.first_cluster)) {
      fat32_set_fat_entry(fs, new_dir_cluster, 0);
      kfree(entry_bytes);
      return false;
    }
  }

  if(lfn_entries > 0) {
    uint8_t checksum = fat32_compute_sfn_checksum(sfn);
    for(size_t i = 0; i < lfn_entries; i++) {
      size_t chunk_index = lfn_entries - 1u - i;
      uint8_t order = (uint8_t)(chunk_index + 1u);
      if(i == 0) {
        order |= 0x40u;
      }
      fat32_lfn_entry_t* lfn_entry = (fat32_lfn_entry_t*)(entry_bytes + i * sizeof(fat32_dir_entry_raw_t));
      fat32_fill_lfn_entry(lfn_entry, order, checksum, name, name_len, chunk_index);
    }
  }

  fat32_dir_entry_raw_t* short_entry =
    (fat32_dir_entry_raw_t*)(entry_bytes + lfn_entries * sizeof(fat32_dir_entry_raw_t));
  memset(short_entry, 0, sizeof(*short_entry));
  memcpy(short_entry->name, sfn, sizeof(short_entry->name));
  short_entry->attr = create_directory ? 0x10u : 0x20u;
  if(create_directory) {
    short_entry->first_cluster_low = (uint16_t)(new_dir_cluster & 0xFFFFu);
    short_entry->first_cluster_high = (uint16_t)((new_dir_cluster >> 16) & 0xFFFFu);
  } else {
    short_entry->first_cluster_low = 0;
    short_entry->first_cluster_high = 0;
  }
  short_entry->file_size = 0;

  bool write_ok = fat32_directory_write_entries(fs,
                                                entry_cluster,
                                                entry_index,
                                                entry_bytes,
                                                total_entries,
                                                used_end_marker);
  kfree(entry_bytes);

  if(!write_ok) {
    if(create_directory && new_dir_cluster != 0) {
      fat32_set_fat_entry(fs, new_dir_cluster, 0);
    }
    return false;
  }

  bool flush_ok = fat32_flush_fat(fs);

  if(out_info) {
    fat32_dir_entry_info_t info;
    if(fat32_find_entry(fs, dir_info.first_cluster, name, &info, false)) {
      *out_info = info;
    } else {
      memset(out_info, 0, sizeof(*out_info));
      strncpy(out_info->name, name, sizeof(out_info->name) - 1u);
      out_info->name[sizeof(out_info->name) - 1u] = '\0';
      out_info->is_directory = create_directory;
      out_info->first_cluster = create_directory ? new_dir_cluster : 0u;
      out_info->size = 0;
      out_info->dir_cluster = entry_cluster;
      out_info->dir_entry_index = entry_index;
    }
  }

  return flush_ok;
}

static bool fat32_directory_is_empty(fat32_fs_t* fs, uint32_t start_cluster) {
  if(fs == NULL) {
    return false;
  }

  uint8_t* buffer = kmalloc(fs->cluster_size_bytes);
  if(buffer == NULL) {
    return false;
  }

  uint32_t cluster = start_cluster;
  bool empty = true;

  while(cluster >= 2u && cluster < fs->max_cluster_index) {
    if(!fat32_read_cluster(fs, cluster, buffer)) {
      empty = false;
      goto cleanup;
    }

    const fat32_dir_entry_raw_t* entries = (const fat32_dir_entry_raw_t*)buffer;
    size_t entries_per_cluster = fs->cluster_size_bytes / sizeof(fat32_dir_entry_raw_t);

    for(size_t idx = 0; idx < entries_per_cluster; idx++) {
      const fat32_dir_entry_raw_t* entry = &entries[idx];
      uint8_t first = entry->name[0];
      if(first == 0x00u) {
        goto cleanup;
      }
      if(first == 0xE5u || entry->attr == 0x0Fu) {
        continue;
      }

      char decoded[32];
      fat32_decode_sfn(entry->name, decoded, sizeof(decoded));
      if(strcmp(decoded, ".") == 0 || strcmp(decoded, "..") == 0) {
        continue;
      }

      empty = false;
      goto cleanup;
    }

    uint32_t next = fat32_get_fat_entry(fs, cluster);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    cluster = next;
  }

cleanup:
  kfree(buffer);
  return empty;
}

typedef struct fat32_entry_pos_t {
  uint32_t cluster;
  uint32_t index;
} fat32_entry_pos_t;

static bool fat32_mark_entry_deleted(fat32_fs_t* fs, fat32_entry_pos_t pos) {
  uint8_t* buffer = kmalloc(fs->cluster_size_bytes);
  if(buffer == NULL) {
    return false;
  }

  if(!fat32_read_cluster(fs, pos.cluster, buffer)) {
    kfree(buffer);
    return false;
  }

  fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;
  entries[pos.index].name[0] = 0xE5u;

  bool ok = fat32_write_cluster(fs, pos.cluster, buffer);
  kfree(buffer);
  return ok;
}

static bool fat32_remove_entry_internal(fat32_fs_t* fs,
                                        uint32_t dir_start_cluster,
                                        const fat32_dir_entry_info_t* info) {
  if(fs == NULL || info == NULL) {
    return false;
  }

  const size_t entries_per_cluster = fs->cluster_size_bytes / sizeof(fat32_dir_entry_raw_t);
  uint8_t* buffer = kmalloc(fs->cluster_size_bytes);
  if(buffer == NULL) {
    return false;
  }

  fat32_entry_pos_t history[32];
  size_t history_len = 0;
  const size_t max_history = sizeof(history) / sizeof(history[0]);
  fat32_entry_pos_t targets[32];
  size_t target_count = 0;

  uint32_t cluster = dir_start_cluster;
  bool found = false;

  while(cluster >= 2u && cluster < fs->max_cluster_index) {
    if(!fat32_read_cluster(fs, cluster, buffer)) {
      goto cleanup_fail;
    }

    fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;

    for(uint32_t idx = 0; idx < entries_per_cluster; idx++) {
      uint8_t first = entries[idx].name[0];
      fat32_entry_pos_t pos = { cluster, idx };

      if(cluster == info->dir_cluster && idx == info->dir_entry_index) {
        size_t required = info->lfn_entries;
        if(required > history_len || required + 1u > sizeof(targets) / sizeof(targets[0])) {
          goto cleanup_fail;
        }

        for(size_t i = 0; i < required; i++) {
          targets[i] = history[history_len - required + i];
        }
        targets[required] = pos;
        target_count = required + 1u;
        found = true;
        goto cleanup;
      }

      if(first == 0x00u) {
        goto cleanup;
      }

      if(history_len == max_history) {
        memmove(history, history + 1, (max_history - 1u) * sizeof(fat32_entry_pos_t));
        history_len--;
      }
      history[history_len++] = pos;
    }

    uint32_t next = fat32_get_fat_entry(fs, cluster);
    if(next >= FAT32_EOC_MARK || next == FAT32_BAD_CLUSTER || next < 2u) {
      break;
    }
    cluster = next;
  }

cleanup:
  kfree(buffer);
  if(!found) {
    return false;
  }

  for(size_t i = 0; i < target_count; i++) {
    if(!fat32_mark_entry_deleted(fs, targets[i])) {
      return false;
    }
  }

  return true;

cleanup_fail:
  kfree(buffer);
  return false;
}

static bool fat32_truncate_file_entry(fat32_fs_t* fs, const char* path) {
  fat32_dir_entry_info_t info;
  if(!fat32_traverse_path(fs, path, &info, false)) {
    return false;
  }

  if(info.is_directory) {
    return false;
  }

  bool fat_dirty = false;
  if(!fat32_adjust_file_size(fs, &info, 0, &fat_dirty)) {
    return false;
  }

  if(!fat32_update_dir_entry(fs, &info, info.first_cluster, 0)) {
    return false;
  }

  if(fat_dirty) {
    return fat32_flush_fat(fs);
  }
  return true;
}

static int fat32_remove_path(fat32_fs_t* fs,
                             const char* path,
                             bool expect_directory,
                             bool allow_directory) {
  if(fs == NULL || path == NULL) {
    return -EINVAL;
  }

  if(strcmp(path, "/") == 0) {
    return -EBUSY;
  }

  char parent_path[256];
  char name[256];
  if(!fat32_split_path(path, parent_path, sizeof(parent_path), name, sizeof(name))) {
    return -EINVAL;
  }

  fat32_dir_entry_info_t parent_info;
  if(!fat32_traverse_path(fs, parent_path, &parent_info, true)) {
    return -ENOENT;
  }

  uint32_t parent_cluster = parent_info.first_cluster ? parent_info.first_cluster : fs->root_cluster;

  fat32_dir_entry_info_t entry_info;
  if(!fat32_find_entry(fs, parent_cluster, name, &entry_info, false)) {
    return -ENOENT;
  }

  if(entry_info.is_directory) {
    if(!allow_directory) {
      return -EISDIR;
    }
    bool dir_empty = fat32_directory_is_empty(fs, entry_info.first_cluster);
    if(!dir_empty) {
      return -ENOTEMPTY;
    }
    if(!expect_directory && !allow_directory) {
      return -EISDIR;
    }
  } else {
    if(expect_directory) {
      return -ENOTDIR;
    }
  }

  if(!fat32_remove_entry_internal(fs, parent_cluster, &entry_info)) {
    return -EIO;
  }

  bool fat_dirty = false;

  if(entry_info.first_cluster != 0) {
    fat32_free_cluster_chain(fs, entry_info.first_cluster);
    fat_dirty = true;
  }

  if(fat_dirty && !fat32_flush_fat(fs)) {
    return -EIO;
  }

  return 0;
}

static int fat32_remove_file_path(fat32_fs_t* fs, const char* path) {
  return fat32_remove_path(fs, path, false, false);
}

static int fat32_remove_directory_path(fat32_fs_t* fs, const char* path) {
  return fat32_remove_path(fs, path, true, true);
}

int fat32_open_adapter(void* fs_ctx, const char* path, int flags, file_t** out_file) {
  (void)out_file;
  if(fs_ctx == NULL || path == NULL) {
    return -1;
  }

  return -ENOSYS;
}

int fat32_unlink_adapter(void* fs_ctx, const char* path) {
  if(fs_ctx == NULL || path == NULL) {
    return -EINVAL;
  }

  fs_mount_t* mount = (fs_mount_t*)fs_ctx;
  fat32_fs_t* fs = &mount->fat32;

  int rc = fat32_remove_file_path(fs, path);
  if(rc == -EISDIR) {
    rc = fat32_remove_directory_path(fs, path);
  }
  return rc;
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

bool fs_file_write(const fs_mount_t* mount,
                   const char* path,
                   size_t offset,
                   const void* buffer,
                   size_t length,
                   size_t* bytes_written) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32) {
    return false;
  }

  if(length == 0) {
    if(bytes_written) {
      *bytes_written = 0;
    }
    return true;
  }

  if(buffer == NULL) {
    return false;
  }

  fat32_fs_t* fs = (fat32_fs_t*)&mount->fat32;
  fat32_dir_entry_info_t info;
  if(!fat32_traverse_path(fs, path, &info, false)) {
    return false;
  }

  if(info.is_directory) {
    return false;
  }

  size_t original_size = info.size;
  size_t target_end = offset + length;
  bool fat_dirty = false;
  if(target_end > info.size) {
    if(!fat32_adjust_file_size(fs, &info, target_end, &fat_dirty)) {
      return false;
    }
  }

  size_t written = 0;
  if(!fat32_write_chain(fs, &info, original_size, offset, buffer, length, &written)) {
    if(bytes_written) {
      *bytes_written = 0;
    }
    return false;
  }

  if(offset + written > info.size) {
    info.size = (uint32_t)(offset + written);
  }

  if(!fat32_update_dir_entry(fs, &info, info.first_cluster, info.size)) {
    return false;
  }

  if(fat_dirty) {
    if(!fat32_flush_fat(fs)) {
      return false;
    }
  }

  if(bytes_written) {
    *bytes_written = written;
  }

  return written == length;
}

bool fs_file_write_all(const fs_mount_t* mount,
                       const char* path,
                       const void* buffer,
                       size_t size) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32) {
    return false;
  }

  if(size > 0 && buffer == NULL) {
    return false;
  }

  fat32_fs_t* fs = (fat32_fs_t*)&mount->fat32;
  fat32_dir_entry_info_t info;
  if(!fat32_traverse_path(fs, path, &info, false)) {
    return false;
  }

  if(info.is_directory) {
    return false;
  }

  size_t original_size = info.size;
  bool fat_dirty = false;
  if(!fat32_adjust_file_size(fs, &info, size, &fat_dirty)) {
    return false;
  }

  size_t written = 0;
  if(size > 0) {
    if(!fat32_write_chain(fs, &info, original_size, 0, buffer, size, &written)) {
      return false;
    }
  }

  if(!fat32_update_dir_entry(fs, &info, info.first_cluster, info.size)) {
    return false;
  }

  if(fat_dirty) {
    if(!fat32_flush_fat(fs)) {
      return false;
    }
  }

  return size == written;
}

bool fs_file_create(const fs_mount_t* mount, const char* path, bool exclusive) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32 || path == NULL) {
    return false;
  }

  fat32_fs_t* fs = (fat32_fs_t*)&mount->fat32;
  return fat32_create_entry(fs, path, exclusive, false, NULL);
}

bool fs_file_truncate(const fs_mount_t* mount, const char* path) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32 || path == NULL) {
    return false;
  }

  fat32_fs_t* fs = (fat32_fs_t*)&mount->fat32;
  return fat32_truncate_file_entry(fs, path);
}

bool fs_directory_create(const fs_mount_t* mount, const char* path, bool exclusive) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32) {
    return false;
  }

  fat32_fs_t* fs = (fat32_fs_t*)&mount->fat32;
  return fat32_create_entry(fs, path, exclusive, true, NULL);
}

bool fs_path_unlink(const fs_mount_t* mount, const char* path) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32) {
    return false;
  }

  fat32_fs_t* fs = (fat32_fs_t*)&mount->fat32;
  return fat32_remove_file_path(fs, path) == 0;
}

bool fs_directory_remove(const fs_mount_t* mount, const char* path) {
  if(mount == NULL || mount->type != FS_TYPE_FAT32) {
    return false;
  }

  fat32_fs_t* fs = (fat32_fs_t*)&mount->fat32;
  return fat32_remove_directory_path(fs, path) == 0;
}
