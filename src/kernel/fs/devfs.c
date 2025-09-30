#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/errno.h>

#include <kernel/char_device.h>
#include <kernel/block_device.h>
#include <kernel/devfs.h>
#include <kernel/file.h>
#include <kernel/vfs.h>

#define DEVFS_NAME_MAX 256
#define DEVFS_MAX_ENTRIES 128
#define DEVFS_PREFIX "/dev/"

typedef struct devfs_seen_entry_t {
  char name[DEVFS_NAME_MAX];
  bool is_directory;
} devfs_seen_entry_t;

typedef struct devfs_emit_ctx {
  devfs_seen_entry_t* table;
  size_t count;
  char prefix[DEVFS_NAME_MAX];
  size_t prefix_len;
  vfs_dir_iter_t iter;
  void* context;
} devfs_emit_ctx_t;

static bool devfs_add_entry(devfs_emit_ctx_t* ctx,
                            const char* name,
                            bool is_directory) {
  if(name == NULL || name[0] == '\0' || ctx->iter == NULL) {
    return true;
  }

  for(size_t idx = 0; idx < ctx->count; idx++) {
    if(strcmp(ctx->table[idx].name, name) == 0) {
      if(is_directory && !ctx->table[idx].is_directory) {
        ctx->table[idx].is_directory = true;
      }
      return true;
    }
  }

  if(ctx->count >= DEVFS_MAX_ENTRIES) {
    return true;
  }

  fs_dir_entry_t entry = {0};
  strncpy(entry.name, name, sizeof(entry.name) - 1);
  entry.is_directory = is_directory;
  entry.size = 0;
  if(!ctx->iter(&entry, ctx->context)) {
    return false;
  }

  strncpy(ctx->table[ctx->count].name, entry.name, sizeof(ctx->table[ctx->count].name) - 1);
  ctx->table[ctx->count].name[sizeof(ctx->table[ctx->count].name) - 1] = '\0';
  ctx->table[ctx->count].is_directory = is_directory;
  ctx->count++;
  return true;
}

static void devfs_normalize_path(const char* path, char* out) {
  if(path == NULL || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
    out[0] = '\0';
    return;
  }

  size_t start = (path[0] == '/') ? 1 : 0;
  strncpy(out, path + start, DEVFS_NAME_MAX - 1);
  out[DEVFS_NAME_MAX - 1] = '\0';

  size_t len = strlen(out);
  while(len > 0 && out[len - 1] == '/') {
    out[--len] = '\0';
  }
}

static void devfs_extract_component(const char* path, char* component, bool* has_more) {
  const char* slash = strchr(path, '/');
  if(slash) {
    size_t len = (size_t)(slash - path);
    if(len >= DEVFS_NAME_MAX) {
      len = DEVFS_NAME_MAX - 1;
    }
    memcpy(component, path, len);
    component[len] = '\0';
    if(has_more) {
      *has_more = true;
    }
  } else {
    strncpy(component, path, DEVFS_NAME_MAX - 1);
    component[DEVFS_NAME_MAX - 1] = '\0';
    if(has_more) {
      *has_more = false;
    }
  }
}

static bool devfs_emit_char_device(char_device_t* device, void* context) {
  devfs_emit_ctx_t* ctx = (devfs_emit_ctx_t*)context;
  const char* full = device->name;
  size_t prefix_len = strlen(DEVFS_PREFIX);
  if(strncmp(full, DEVFS_PREFIX, prefix_len) != 0) {
    return true;
  }

  const char* rel = full + prefix_len;
  if(rel[0] == '\0') {
    return true;
  }

  if(ctx->prefix_len > 0) {
    if(strncmp(rel, ctx->prefix, ctx->prefix_len) != 0) {
      return true;
    }
    rel += ctx->prefix_len;
    if(rel[0] == '/') {
      rel++;
    }
    if(rel[0] == '\0') {
      return true;
    }
  }

  char component[DEVFS_NAME_MAX];
  bool has_more = false;
  devfs_extract_component(rel, component, &has_more);
  if(component[0] == '\0') {
    return true;
  }

  if(!devfs_add_entry(ctx, component, has_more)) {
    return false;
  }
  return true;
}

static bool devfs_emit_block_device(block_device_t* device, void* context) {
  devfs_emit_ctx_t* ctx = (devfs_emit_ctx_t*)context;
  if(ctx->prefix_len != 0) {
    return true;
  }
  if(device->name[0] == '\0') {
    return true;
  }
  return devfs_add_entry(ctx, device->name, false);
}

static bool devfs_list(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context) {
  (void)fs_ctx;
  if(iter == NULL) {
    return false;
  }

  devfs_seen_entry_t table[DEVFS_MAX_ENTRIES] = {0};
  devfs_emit_ctx_t ctx = {
    .table = table,
    .count = 0,
    .prefix = {0},
    .prefix_len = 0,
    .iter = iter,
    .context = context,
  };

  devfs_normalize_path(path, ctx.prefix);
  ctx.prefix_len = strlen(ctx.prefix);
  if(ctx.prefix_len > 0 && ctx.prefix[ctx.prefix_len - 1] != '/') {
    ctx.prefix[ctx.prefix_len] = '/';
    ctx.prefix[++ctx.prefix_len] = '\0';
  }

  char_device_for_each(devfs_emit_char_device, &ctx);
  block_device_for_each(devfs_emit_block_device, &ctx);
  return true;
}

static int devfs_open(void* fs_ctx, const char* path, int flags, file_t** out_file) {
  (void)fs_ctx;
  (void)flags;
  if(path == NULL) {
    return -EINVAL;
  }
  if(path[0] == '/' && path[1] == '\0') {
    return -EISDIR;
  }

  char full[DEVFS_NAME_MAX + sizeof(DEVFS_PREFIX)] = {0};
  size_t prefix_len = strlen(DEVFS_PREFIX);
  memcpy(full, DEVFS_PREFIX, prefix_len);

  const char* rest = path;
  if(path[0] == '/') {
    rest = path + 1;
  }

  size_t rest_len = strlen(rest);
  if(prefix_len + rest_len >= sizeof(full)) {
    return -ENAMETOOLONG;
  }
  memcpy(full + prefix_len, rest, rest_len + 1);

  return char_device_open(full, 0, out_file);
}

static const vfs_fs_driver_t devfs_driver = {
  .list = devfs_list,
  .read = NULL,
  .read_all = NULL,
  .open = devfs_open,
  .unlink = NULL,
  .destroy = NULL,
};

bool devfs_mount(void) {
  static bool mounted = false;
  if(mounted) {
    return true;
  }
  if(!vfs_mount("/dev", &devfs_driver, NULL)) {
    return false;
  }
  mounted = true;
  return true;
}
