#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

#include <kernel/block_device.h>

#define FAT32_TEST_CLUSTER_COUNT 64u

typedef struct fat32_test_disk_t {
  uint8_t* data;
  size_t   size;
  uint32_t block_size;
} fat32_test_disk_t;

static bool test_disk_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count);
static bool test_disk_write(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count);
static bool test_disk_flush(block_device_t* device);

static const block_device_ops_t test_disk_ops = {
  .read_blocks = test_disk_read,
  .write_blocks = test_disk_write,
  .flush = test_disk_flush,
};

#include "../src/kernel/fs/fat32.c"

typedef struct fat32_test_env_t {
  fat32_fs_t        fs;
  block_device_t    device;
  fat32_test_disk_t disk;
} fat32_test_env_t;

static bool test_disk_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count) {
  fat32_test_disk_t* disk = (fat32_test_disk_t*)device->driver_ctx;
  size_t offset = (size_t)lba * disk->block_size;
  size_t bytes = block_count * (size_t)disk->block_size;
  if(offset + bytes > disk->size) {
    TEST_FAIL_MESSAGE("disk read overflow");
    return false;
  }
  memcpy(buffer, disk->data + offset, bytes);
  return true;
}

static bool test_disk_write(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count) {
  fat32_test_disk_t* disk = (fat32_test_disk_t*)device->driver_ctx;
  size_t offset = (size_t)lba * disk->block_size;
  size_t bytes = block_count * (size_t)disk->block_size;
  if(offset + bytes > disk->size) {
    TEST_FAIL_MESSAGE("disk write overflow");
    return false;
  }
  memcpy(disk->data + offset, buffer, bytes);
  return true;
}

static bool test_disk_flush(block_device_t* device) {
  (void)device;
  return true;
}

static void fat32_test_env_init(fat32_test_env_t* env, uint32_t cluster_count) {
  memset(env, 0, sizeof(*env));

  env->disk.block_size = 512u;
  env->disk.size = (size_t)env->disk.block_size * (2u + cluster_count);
  env->disk.data = (uint8_t*)malloc(env->disk.size);
  TEST_ASSERT_NOT_NULL(env->disk.data);
  memset(env->disk.data, 0, env->disk.size);

  env->device.block_size = env->disk.block_size;
  env->device.block_count = env->disk.size / env->disk.block_size;
  env->device.driver_ctx = &env->disk;
  env->device.ops = &test_disk_ops;

  fat32_fs_t* fs = &env->fs;
  memset(fs, 0, sizeof(*fs));
  fs->device = &env->device;
  fs->partition_start_lba = 0;
  fs->partition_block_count = env->device.block_count;
  fs->bytes_per_sector = env->disk.block_size;
  fs->sectors_per_cluster = 1u;
  fs->reserved_sectors = 1u;
  fs->num_fats = 1u;
  fs->fat_size_sectors = 1u;
  fs->root_cluster = 2u;
  fs->fat_start_lba = fs->reserved_sectors;
  fs->data_start_lba = fs->fat_start_lba + fs->fat_size_sectors;
  fs->cluster_size_bytes = fs->bytes_per_sector * fs->sectors_per_cluster;
  fs->fat_table_bytes = cluster_count * sizeof(uint32_t);
  fs->max_cluster_index = cluster_count;

  fs->fat_table = kmalloc(fs->fat_table_bytes);
  TEST_ASSERT_NOT_NULL(fs->fat_table);
  memset(fs->fat_table, 0, fs->fat_table_bytes);

  uint32_t* fat = (uint32_t*)fs->fat_table;
  fat[0] = FAT32_EOC_MARK;
  fat[1] = FAT32_EOC_MARK;
  fat[fs->root_cluster] = FAT32_EOC_MARK;
}

static void fat32_test_env_destroy(fat32_test_env_t* env) {
  if(env->fs.fat_table) {
    kfree(env->fs.fat_table);
    env->fs.fat_table = NULL;
  }
  free(env->disk.data);
  env->disk.data = NULL;
}

static void set_short_entry_cluster(fat32_dir_entry_raw_t* entry, uint32_t cluster) {
  entry->first_cluster_low = (uint16_t)(cluster & 0xFFFFu);
  entry->first_cluster_high = (uint16_t)((cluster >> 16) & 0xFFFFu);
}

static void write_directory_entry(fat32_test_env_t* env,
                                  uint32_t parent_cluster,
                                  size_t* next_index,
                                  const char* name,
                                  bool is_directory,
                                  uint32_t* out_directory_cluster) {
  uint8_t sfn[11];
  bool requires_lfn = false;
  TEST_ASSERT_TRUE(fat32_generate_sfn(&env->fs, parent_cluster, name, sfn, &requires_lfn));

  size_t name_len = strlen(name);
  size_t lfn_entries = requires_lfn ? ((name_len + 12u) / 13u) : 0u;
  size_t total_entries = lfn_entries + 1u;

  uint8_t* raw_entries = kmalloc(total_entries * sizeof(fat32_dir_entry_raw_t));
  TEST_ASSERT_NOT_NULL(raw_entries);
  memset(raw_entries, 0, total_entries * sizeof(fat32_dir_entry_raw_t));

  uint32_t new_dir_cluster = 0;
  if(is_directory) {
    new_dir_cluster = fat32_allocate_cluster(&env->fs);
    TEST_ASSERT_NOT_EQUAL(0u, new_dir_cluster);
    TEST_ASSERT_TRUE(fat32_setup_directory_cluster(&env->fs, new_dir_cluster, parent_cluster));
    if(out_directory_cluster) {
      *out_directory_cluster = new_dir_cluster;
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
      fat32_lfn_entry_t* lfn_entry =
        (fat32_lfn_entry_t*)(raw_entries + i * sizeof(fat32_dir_entry_raw_t));
      fat32_fill_lfn_entry(lfn_entry, order, checksum, name, name_len, chunk_index);
    }
  }

  fat32_dir_entry_raw_t* short_entry =
    (fat32_dir_entry_raw_t*)(raw_entries + lfn_entries * sizeof(fat32_dir_entry_raw_t));
  memset(short_entry, 0, sizeof(*short_entry));
  memcpy(short_entry->name, sfn, sizeof(short_entry->name));
  short_entry->attr = is_directory ? 0x10u : 0x20u;
  if(is_directory) {
    set_short_entry_cluster(short_entry, new_dir_cluster);
  }
  short_entry->file_size = 0;

  TEST_ASSERT_TRUE(fat32_directory_write_entries(&env->fs,
                                                 parent_cluster,
                                                 (uint32_t)(*next_index),
                                                 raw_entries,
                                                 total_entries,
                                                 true));

  kfree(raw_entries);
  *next_index += total_entries;
  TEST_ASSERT_TRUE(fat32_flush_fat(&env->fs));
}

void setUp(void) {}
void tearDown(void) {}

void test_create_long_filename_file(void) {
  fat32_test_env_t env;
  fat32_test_env_init(&env, FAT32_TEST_CLUSTER_COUNT);

  size_t next_index = 0;
  write_directory_entry(&env, env.fs.root_cluster, &next_index, "LongFileName123.txt", false, NULL);

  uint8_t buffer[512];
  TEST_ASSERT_TRUE(fat32_read_cluster(&env.fs, env.fs.root_cluster, buffer));
  fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;
  TEST_ASSERT_EQUAL_HEX8(0x0F, entries[0].attr);
  TEST_ASSERT_EQUAL_HEX8(0x0F, entries[1].attr);
  fat32_dir_entry_raw_t* short_entry = &entries[2];
  TEST_ASSERT_EQUAL_HEX8(0x20, short_entry->attr);
  char decoded[32];
  fat32_decode_sfn(short_entry->name, decoded, sizeof(decoded));
  TEST_ASSERT_EQUAL_STRING("LONGFILE.TXT", decoded);

  char lfn_buffer[256];
  fat32_reset_lfn(lfn_buffer, sizeof(lfn_buffer));
  fat32_append_lfn_segment(lfn_buffer, sizeof(lfn_buffer), (const fat32_lfn_entry_t*)&entries[0]);
  fat32_append_lfn_segment(lfn_buffer, sizeof(lfn_buffer), (const fat32_lfn_entry_t*)&entries[1]);
  TEST_ASSERT_EQUAL_STRING("LongFileName123.txt", lfn_buffer);

  fat32_test_env_destroy(&env);
}

void test_create_directory_with_long_name(void) {
  fat32_test_env_t env;
  fat32_test_env_init(&env, FAT32_TEST_CLUSTER_COUNT);

  size_t root_index = 0;
  uint32_t dir_cluster = 0;
  write_directory_entry(&env, env.fs.root_cluster, &root_index, "Projects Archive", true, &dir_cluster);
  TEST_ASSERT_NOT_EQUAL(0u, dir_cluster);

  uint8_t dir_buffer[512];
  TEST_ASSERT_TRUE(fat32_read_cluster(&env.fs, dir_cluster, dir_buffer));
  const fat32_dir_entry_raw_t* dir_entries = (const fat32_dir_entry_raw_t*)dir_buffer;
  TEST_ASSERT_EQUAL_CHAR('.', (char)dir_entries[0].name[0]);
  TEST_ASSERT_EQUAL_CHAR('.', (char)dir_entries[1].name[0]);
  TEST_ASSERT_EQUAL_CHAR('.', (char)dir_entries[1].name[1]);
  TEST_ASSERT_EQUAL_UINT16(dir_cluster & 0xFFFFu, dir_entries[0].first_cluster_low);
  TEST_ASSERT_EQUAL_UINT16(env.fs.root_cluster & 0xFFFFu, dir_entries[1].first_cluster_low);

  size_t child_index = 0;
  write_directory_entry(&env, dir_cluster, &child_index, "readme.md", false, NULL);

  fat32_test_env_destroy(&env);
}

void test_unique_short_name_generation(void) {
  fat32_test_env_t env;
  fat32_test_env_init(&env, FAT32_TEST_CLUSTER_COUNT);

  size_t next_index = 0;
  write_directory_entry(&env, env.fs.root_cluster, &next_index, "samplefile.txt", false, NULL);
  write_directory_entry(&env, env.fs.root_cluster, &next_index, "samplefile2.txt", false, NULL);

  uint8_t buffer[512];
  TEST_ASSERT_TRUE(fat32_read_cluster(&env.fs, env.fs.root_cluster, buffer));
  fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;

  const uint8_t expected[11] = { 'S','A','M','P','L','E','~','1','T','X','T' };
  bool found = false;
  for(size_t idx = 0; idx < env.fs.cluster_size_bytes / sizeof(fat32_dir_entry_raw_t); idx++) {
    if(entries[idx].name[0] == 0x00u) {
      break;
    }
    if(entries[idx].attr == 0x0Fu || entries[idx].name[0] == 0xE5u) {
      continue;
    }
    if(memcmp(entries[idx].name, expected, sizeof(expected)) == 0) {
      found = true;
      break;
    }
  }

  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_TRUE(fat32_traverse_path(&env.fs, "/samplefile2.txt", NULL, false));

  fat32_test_env_destroy(&env);
}

void test_remove_file_marks_entries_deleted(void) {
  fat32_test_env_t env;
  fat32_test_env_init(&env, FAT32_TEST_CLUSTER_COUNT);

  size_t next_index = 0;
  write_directory_entry(&env,
                        env.fs.root_cluster,
                        &next_index,
                        "LongFileName123.txt",
                        false,
                        NULL);

  fat32_dir_entry_info_t info;
  TEST_ASSERT_TRUE(fat32_find_entry(&env.fs, env.fs.root_cluster, "LongFileName123.txt", &info, false));

  int rc = fat32_remove_file_path(&env.fs, "/LongFileName123.txt");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "fat32_remove_file_path failed");

  uint8_t buffer[512];
  TEST_ASSERT_TRUE(fat32_read_cluster(&env.fs, env.fs.root_cluster, buffer));
  fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;
  TEST_ASSERT_EQUAL_UINT8(0xE5u, entries[0].name[0]);
  TEST_ASSERT_EQUAL_UINT8(0xE5u, entries[1].name[0]);
  TEST_ASSERT_EQUAL_UINT8(0xE5u, entries[2].name[0]);

  fat32_test_env_destroy(&env);
}

void test_remove_directory_requires_empty(void) {
  fat32_test_env_t env;
  fat32_test_env_init(&env, FAT32_TEST_CLUSTER_COUNT);

  size_t root_index = 0;
  uint32_t projects_cluster = 0;
  write_directory_entry(&env,
                        env.fs.root_cluster,
                        &root_index,
                        "Projects",
                        true,
                        &projects_cluster);

  size_t dir_index = 0;
  write_directory_entry(&env,
                        projects_cluster,
                        &dir_index,
                        "readme.txt",
                        false,
                        NULL);

  fat32_dir_entry_info_t project_info;
  TEST_ASSERT_TRUE(fat32_find_entry(&env.fs, env.fs.root_cluster, "Projects", &project_info, false));
  int rc = fat32_remove_directory_path(&env.fs, "/Projects");
  TEST_ASSERT_EQUAL_INT(-ENOTEMPTY, rc);

  rc = fat32_remove_file_path(&env.fs, "/Projects/readme.txt");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "fat32_remove_file_path failed");
  rc = fat32_remove_directory_path(&env.fs, "/Projects");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "fat32_remove_directory_path failed");

  uint8_t buffer[512];
  TEST_ASSERT_TRUE(fat32_read_cluster(&env.fs, env.fs.root_cluster, buffer));
  fat32_dir_entry_raw_t* entries = (fat32_dir_entry_raw_t*)buffer;

  size_t expected_deleted = (size_t)project_info.lfn_entries + 1u;
  for(size_t idx = 0; idx < expected_deleted; idx++) {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xE5u,
                                    entries[idx].name[0],
                                    "directory entries should be marked deleted");
  }

  fat32_test_env_destroy(&env);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_create_long_filename_file);
  RUN_TEST(test_create_directory_with_long_name);
  RUN_TEST(test_unique_short_name_generation);
  RUN_TEST(test_remove_file_marks_entries_deleted);
  RUN_TEST(test_remove_directory_requires_empty);
  return UNITY_END();
}
