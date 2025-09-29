#include <kernel/vfs.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <kernel/block_device.h>
#include <kernel/input/keyboard.h>
#include <kernel/fs.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>

typedef struct vfs_mount_entry_t {
  char                        path[128];
  size_t                      path_len;
  const vfs_fs_driver_t*      driver;
  void*                       fs_ctx;
  struct vfs_mount_entry_t*   next;
} vfs_mount_entry_t;

typedef struct vfs_file_buffer_t {
  uint8_t* data;
  size_t   size;
  size_t   offset;
} vfs_file_buffer_t;

static const file_ops_t vfs_file_ops;

static vfs_mount_entry_t* vfs_mounts = NULL;
static kmutex_t           vfs_lock;
static bool               vfs_initialized = false;

static bool vfs_normalize_path(const char* path, char* out, size_t out_size, size_t* out_len) {
  if(path == NULL || out == NULL || out_size == 0) {
    return false;
  }

  size_t len = strlen(path);
  if(len == 0) {
    if(out_size < 2) {
      return false;
    }
    out[0] = '/';
    out[1] = '\0';
    if(out_len) {
      *out_len = 1;
    }
    return true;
  }

  size_t pos = 0;
  if(path[0] != '/') {
    if(out_size < len + 2) {
      return false;
    }
    out[pos++] = '/';
  }

  for(size_t i = 0; i < len && pos + 1 < out_size; i++) {
    char ch = path[i];
    if(ch == '\\') {
      ch = '/';
    }
    if(pos > 0 && out[pos - 1] == '/' && ch == '/') {
      continue;
    }
    out[pos++] = ch;
  }

  if(pos == 0) {
    out[pos++] = '/';
  }

  if(pos > 1 && out[pos - 1] == '/') {
    pos--;
  }

  if(pos >= out_size) {
    return false;
  }

  out[pos] = '\0';
  if(out_len) {
    *out_len = pos;
  }
  return true;
}

bool vfs_init(void) {
  if(vfs_initialized) {
    return true;
  }
  kmutex_init(&vfs_lock);
  vfs_mounts = NULL;
  vfs_initialized = true;
  return true;
}

void vfs_shutdown(void) {
  if(!vfs_initialized) {
    return;
  }

  kmutex_lock(&vfs_lock);
  vfs_mount_entry_t* entry = vfs_mounts;
  while(entry) {
    vfs_mount_entry_t* next = entry->next;
    if(entry->driver && entry->driver->destroy) {
      entry->driver->destroy(entry->fs_ctx);
    }
    kfree(entry);
    entry = next;
  }
  vfs_mounts = NULL;
  kmutex_unlock(&vfs_lock);
  vfs_initialized = false;
}

static vfs_mount_entry_t* vfs_find_mount_locked(const char* path, size_t path_len, size_t* out_prefix_len) {
  vfs_mount_entry_t* best = NULL;
  size_t best_len = 0;

  for(vfs_mount_entry_t* entry = vfs_mounts; entry != NULL; entry = entry->next) {
    if(path_len < entry->path_len) {
      continue;
    }
    if(strncmp(path, entry->path, entry->path_len) != 0) {
      continue;
    }
    if(path_len > entry->path_len) {
      if(!(entry->path_len == 1 && entry->path[0] == '/')) {
        if(path[entry->path_len] != '/') {
          continue;
        }
      }
    }
    if(entry->path_len > best_len) {
      best = entry;
      best_len = entry->path_len;
    }
  }

  if(out_prefix_len) {
    *out_prefix_len = best_len;
  }
  return best;
}

bool vfs_mount(const char* path, const vfs_fs_driver_t* driver, void* fs_ctx) {
  if(!vfs_initialized && !vfs_init()) {
    return false;
  }

  if(path == NULL || driver == NULL) {
    return false;
  }

  char normalized[128];
  size_t norm_len = 0;
  if(!vfs_normalize_path(path, normalized, sizeof(normalized), &norm_len)) {
    return false;
  }

  kmutex_lock(&vfs_lock);
  size_t prefix_len = 0;
  vfs_mount_entry_t* existing = vfs_find_mount_locked(normalized, norm_len, &prefix_len);
  if(existing && prefix_len == norm_len) {
    kmutex_unlock(&vfs_lock);
    return false;
  }

  vfs_mount_entry_t* entry = kmalloc(sizeof(vfs_mount_entry_t));
  if(entry == NULL) {
    kmutex_unlock(&vfs_lock);
    return false;
  }

  memset(entry, 0, sizeof(*entry));
  memcpy(entry->path, normalized, norm_len + 1);
  entry->path_len = norm_len;
  entry->driver = driver;
  entry->fs_ctx = fs_ctx;
  entry->next = vfs_mounts;
  vfs_mounts = entry;
  kmutex_unlock(&vfs_lock);

  serial_printf("vfs: mounted %s\n", entry->path);
  return true;
}

bool vfs_mount_root(const vfs_fs_driver_t* driver, void* fs_ctx) {
  return vfs_mount("/", driver, fs_ctx);
}

static bool vfs_resolve(const char* path,
                        const vfs_fs_driver_t** driver_out,
                        void** fs_ctx_out,
                        char* relative,
                        size_t relative_size) {
  if(!vfs_initialized) {
    return false;
  }

  char normalized[128];
  size_t norm_len = 0;
  if(!vfs_normalize_path(path, normalized, sizeof(normalized), &norm_len)) {
    return false;
  }

  kmutex_lock(&vfs_lock);
  size_t prefix_len = 0;
  vfs_mount_entry_t* entry = vfs_find_mount_locked(normalized, norm_len, &prefix_len);
  kmutex_unlock(&vfs_lock);

  if(entry == NULL || entry->driver == NULL) {
    return false;
  }

  if(driver_out) {
    *driver_out = entry->driver;
  }
  if(fs_ctx_out) {
    *fs_ctx_out = entry->fs_ctx;
  }

  if(relative && relative_size > 0) {
    const char* remainder = normalized + prefix_len;
    if(entry->path_len == 1 && entry->path[0] == '/') {
      remainder = normalized;
    }

    if(*remainder == '\0') {
      strncpy(relative, "/", relative_size);
      relative[relative_size - 1] = '\0';
    } else {
      size_t rem_len = strlen(remainder);
      if(rem_len + 1 > relative_size) {
        return false;
      }
      memcpy(relative, remainder, rem_len + 1);
    }
  }

  return true;
}

bool vfs_list(const char* path, vfs_dir_iter_t iter, void* context) {
  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[128];

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative))) {
    return false;
  }

  if(driver->list == NULL) {
    return false;
  }
  return driver->list(fs_ctx, relative, iter, context);
}

bool vfs_read(const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read) {
  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[128];

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative))) {
    return false;
  }
  if(driver->read == NULL) {
    return false;
  }
  return driver->read(fs_ctx, relative, offset, buffer, length, bytes_read);
}

bool vfs_read_all(const char* path, void** out_buffer, size_t* out_size) {
  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[128];

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative))) {
    return false;
  }
  if(driver->read_all == NULL) {
    return false;
  }
  return driver->read_all(fs_ctx, relative, out_buffer, out_size);
}

static int64_t vfs_file_read_impl(file_t* file, void* buffer, size_t length) {
  if(file == NULL || buffer == NULL || length == 0) {
    return 0;
  }

  vfs_file_buffer_t* ctx = (vfs_file_buffer_t*)file->private_data;
  if(ctx == NULL || ctx->data == NULL) {
    return -EINVAL;
  }

  if(ctx->offset >= ctx->size) {
    return 0;
  }

  size_t remaining = ctx->size - ctx->offset;
  size_t to_copy = remaining < length ? remaining : length;
  memcpy(buffer, ctx->data + ctx->offset, to_copy);
  ctx->offset += to_copy;
  return (int64_t)to_copy;
}

static int vfs_file_close_impl(file_t* file) {
  if(file == NULL) {
    return 0;
  }
  vfs_file_buffer_t* ctx = (vfs_file_buffer_t*)file->private_data;
  if(ctx) {
    if(ctx->data) {
    kfree(ctx->data);
    }
    kfree(ctx);
    file->private_data = NULL;
  }
  return 0;
}

static int64_t vfs_file_seek_impl(file_t* file, int64_t offset, int whence) {
  if(file == NULL) {
    return -EINVAL;
  }

  vfs_file_buffer_t* ctx = (vfs_file_buffer_t*)file->private_data;
  if(ctx == NULL) {
    return -EINVAL;
  }

  int64_t base = 0;
  switch(whence) {
    case SEEK_SET:
      base = 0;
      break;
    case SEEK_CUR:
      base = (int64_t)ctx->offset;
      break;
    case SEEK_END:
      base = (int64_t)ctx->size;
      break;
    default:
      return -EINVAL;
  }

  int64_t new_offset = base + offset;
  if(new_offset < 0 || (uint64_t)new_offset > ctx->size) {
    return -EINVAL;
  }

  ctx->offset = (size_t)new_offset;
  return new_offset;
}

static const file_ops_t vfs_file_ops = {
  .read = vfs_file_read_impl,
  .write = NULL,
  .close = vfs_file_close_impl,
  .seek = vfs_file_seek_impl,
};

static int vfs_open_buffered(const char* path, file_t** out_file) {
  void* data = NULL;
  size_t size = 0;
  if(!vfs_read_all(path, &data, &size)) {
    if(data) {
      kfree(data);
    }
    return -ENOENT;
  }

  vfs_file_buffer_t* ctx = kmalloc(sizeof(vfs_file_buffer_t));
  if(ctx == NULL) {
    kfree(data);
    return -ENOMEM;
  }

  ctx->data = (uint8_t*)data;
  ctx->size = size;
  ctx->offset = 0;

  file_t* file = file_create(&vfs_file_ops, ctx, FILE_MODE_READ);
  if(file == NULL) {
    if(ctx->data) {
      kfree(ctx->data);
    }
    kfree(ctx);
    return -ENOMEM;
  }

  *out_file = file;
  return 0;
}

int vfs_open(const char* path, int flags, file_t** out_file) {
  if(path == NULL || out_file == NULL) {
    return -EINVAL;
  }

  *out_file = NULL;

  if(strcmp(path, "/dev/input/kbd") == 0) {
    file_t* dev = keyboard_device_open();
    if(dev == NULL) {
      return -ENOMEM;
    }
    *out_file = dev;
    return 0;
  }

  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[128];

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative))) {
    return -ENOENT;
  }

  if(driver->open) {
    int rc = driver->open(fs_ctx, relative, flags, out_file);
    if(rc != -ENOSYS) {
      return rc;
    }
  }

  int accmode = flags & O_ACCMODE;
  if(accmode == O_WRONLY || accmode == O_RDWR) {
    return -EROFS;
  }

  if(flags & (O_CREAT | O_TRUNC | O_APPEND | O_EXCL)) {
    return -EROFS;
  }

  if(flags & O_DIRECTORY) {
    return -ENOSYS;
  }

  return vfs_open_buffered(path, out_file);
}

static bool fat32_list_adapter(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context) {
  const fs_mount_t* mount = (const fs_mount_t*)fs_ctx;
  bool ok = fs_list_directory(mount, path, iter, context);
  if(!ok && path != NULL && path[0] == '/' && path[1] != '\0') {
    ok = fs_list_directory(mount, path + 1, iter, context);
  }
  if(!ok) {
    serial_printf("fat32: list failed for '%s'\n", path);
  }
  return ok;
}

static bool fat32_read_adapter(void* fs_ctx,
                               const char* path,
                               size_t offset,
                               void* buffer,
                               size_t length,
                               size_t* bytes_read) {
  const fs_mount_t* mount = (const fs_mount_t*)fs_ctx;
  bool ok = fs_file_read(mount, path, offset, buffer, length, bytes_read);
  if(!ok && path != NULL && path[0] == '/' && path[1] != '\0') {
    ok = fs_file_read(mount, path + 1, offset, buffer, length, bytes_read);
  }
  return ok;
}

static bool fat32_read_all_adapter(void* fs_ctx, const char* path, void** out_buffer, size_t* out_size) {
  const fs_mount_t* mount = (const fs_mount_t*)fs_ctx;
  bool ok = fs_file_read_all(mount, path, out_buffer, out_size);
  if(!ok && path != NULL && path[0] == '/' && path[1] != '\0') {
    ok = fs_file_read_all(mount, path + 1, out_buffer, out_size);
  }
  return ok;
}

static void fat32_destroy_adapter(void* fs_ctx) {
  fs_unmount((fs_mount_t*)fs_ctx);
}

static const vfs_fs_driver_t fat32_driver = {
  .list = fat32_list_adapter,
  .read = fat32_read_adapter,
  .read_all = fat32_read_all_adapter,
  .open = NULL,
  .unlink = NULL,
  .destroy = fat32_destroy_adapter,
};

bool vfs_mount_fat32_root(block_device_t* device) {
  static bool mounted = false;
  if(mounted) {
    return true;
  }
  if(device == NULL) {
    return false;
  }

  fs_mount_t* mount = NULL;
  if(!fs_mount_fat32_first(device, &mount)) {
    return false;
  }

  if(!vfs_mount_root(&fat32_driver, mount)) {
    fs_unmount(mount);
    return false;
  }

  mounted = true;
  return true;
}
