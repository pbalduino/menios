#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <kernel/block_device.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

extern const uint8_t user_demo_elf_start[];
extern const uint8_t user_demo_elf_end[];

typedef struct user_demo_spec_t {
  const char* name;
  uint8_t     priority;
} user_demo_spec_t;

static const user_demo_spec_t demo_specs[] = {
  { "user_demo_low",     PROC_PRIO_LOW },
  { "user_demo_normal",  PROC_PRIO_NORMAL },
  { "user_demo_high",    PROC_PRIO_HIGH }
};

#define USER_DEMO_COUNT (sizeof(demo_specs) / sizeof(demo_specs[0]))

static void user_demo_block_probe(void) {
  block_device_t* device = block_device_first();
  while(device != NULL) {
    if(device->block_size != 0 && strncmp(device->name, "sata", 4) == 0) {
      break;
    }
    device = block_device_next(device);
  }

  if(device == NULL) {
    serial_printf("user_demo_launch: no SATA device available for probe\n");
    return;
  }

  size_t block_size = device->block_size ? device->block_size : 512u;
  uint8_t* buffer = kmalloc(block_size);
  if(buffer == NULL) {
    serial_printf("user_demo_launch: failed to allocate SATA probe buffer\n");
    return;
  }

  bool ok = block_device_read(device, 0, buffer, 1);
  if(ok) {
    serial_printf("user_demo_launch: '%s' LBA0 first 16 bytes: ", device->name);
    size_t preview = block_size < 16 ? block_size : 16;
    for(size_t i = 0; i < preview; i++) {
      serial_printf("%02x%s", buffer[i], (i + 1 < preview) ? " " : "");
    }
    serial_printf("\n");
  } else {
    serial_printf("user_demo_launch: SATA read failed on '%s'\n", device->name);
  }

  kfree(buffer);
}

typedef struct user_demo_list_ctx_t {
  size_t      depth;
  size_t      max_depth;
  const char* parent_path;
} user_demo_list_ctx_t;

static void user_demo_print_indent(size_t depth) {
  for(size_t i = 0; i < depth; i++) {
    serial_printf("  ");
  }
}

static bool user_demo_build_child_path(char* buffer,
                                       size_t buffer_size,
                                       const char* parent,
                                       const char* name) {
  if(buffer == NULL || parent == NULL || name == NULL) {
    return false;
  }

  size_t name_len = strlen(name);
  if(parent[0] == '\0' || (parent[0] == '/' && parent[1] == '\0')) {
    if(name_len + 2 > buffer_size) {
      return false;
    }
    buffer[0] = '/';
    memcpy(buffer + 1, name, name_len);
    buffer[1 + name_len] = '\0';
  } else {
    size_t parent_len = strlen(parent);
    if(parent_len + 1 + name_len + 1 > buffer_size) {
      return false;
    }
    memcpy(buffer, parent, parent_len);
    buffer[parent_len] = '/';
    memcpy(buffer + parent_len + 1, name, name_len);
    buffer[parent_len + 1 + name_len] = '\0';
  }

  return true;
}

static void user_demo_list_directory(const char* path,
                                     size_t depth,
                                     size_t max_depth);

static bool user_demo_dir_iter(const vfs_dir_entry_t* entry, void* context) {
  user_demo_list_ctx_t* ctx = (user_demo_list_ctx_t*)context;
  if(ctx == NULL || entry == NULL) {
    return false;
  }

  if(entry->is_directory &&
     (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0)) {
    return true;
  }

  const char* type = entry->is_directory ? "<DIR>" : "<FILE>";
  user_demo_print_indent(ctx->depth + 1);
  serial_printf("user_demo_fs: %s %s (%u bytes)\n",
                type,
                entry->name,
                entry->size);

  if(!entry->is_directory) {
    return true;
  }

  if(ctx->depth + 1 >= ctx->max_depth) {
    return true;
  }

  char child_path[256];
  if(!user_demo_build_child_path(child_path,
                                 sizeof(child_path),
                                 ctx->parent_path,
                                 entry->name)) {
    user_demo_print_indent(ctx->depth + 1);
    serial_printf("user_demo_fs: <path too long, skipping>\n");
    return true;
  }

  user_demo_list_directory(child_path, ctx->depth + 1, ctx->max_depth);
  return true;
}

static void user_demo_list_directory(const char* path,
                                     size_t depth,
                                     size_t max_depth) {
  if(path == NULL) {
    return;
  }

  user_demo_print_indent(depth);
  serial_printf("user_demo_fs: dir %s\n", path);

  user_demo_list_ctx_t ctx = {
    .depth = depth,
    .max_depth = max_depth,
    .parent_path = path,
  };

  if(!vfs_list(path, user_demo_dir_iter, &ctx)) {
    user_demo_print_indent(depth + 1);
    serial_printf("user_demo_fs: <failed to list %s>\n", path);
  }
}

static void user_demo_filesystem_probe(void) {
  block_device_t* device = block_device_first();
  while(device != NULL) {
    if(device->block_size != 0 && strncmp(device->name, "sata", 4) == 0) {
      break;
    }
    device = block_device_next(device);
  }

  if(device == NULL) {
    serial_printf("user_demo_launch: no SATA device available for filesystem demo\n");
    return;
  }

  if(!vfs_mount_fat32_root(device)) {
    serial_printf("user_demo_launch: failed to mount FAT32 filesystem on '%s'\n", device->name);
    return;
  }

  serial_printf("user_demo_launch: mounted FAT32 filesystem on '%s'\n", device->name);
  user_demo_list_directory("/", 0, 2);
}

void user_demo_launch(void) {
  size_t code_size = (size_t)(user_demo_elf_end - user_demo_elf_start);
  if(code_size == 0) {
    serial_printf("user_demo_launch: stub size is zero\n");
    return;
  }

  serial_printf("user_demo_launch: scheduling %lu user demo processes\n",
                (unsigned long)USER_DEMO_COUNT);

  for(size_t i = 0; i < USER_DEMO_COUNT; i++) {
    proc_info_p proc = kmalloc(sizeof(proc_info_t));
    if(proc == NULL) {
      serial_printf("user_demo_launch: failed to allocate proc_info (%zu)\n", i);
      return;
    }
    memset(proc, 0, sizeof(proc_info_t));

    proc_create_user(proc,
                     demo_specs[i].name,
                     user_demo_elf_start,
                     code_size,
                     (void*)(uintptr_t)i);

    proc_set_priority(proc, demo_specs[i].priority);
    serial_printf("user_demo_launch: queued %s with priority %u\n",
                  demo_specs[i].name,
                  demo_specs[i].priority);

    proc_execute(proc);
  }

  user_demo_block_probe();
  user_demo_filesystem_probe();
}
