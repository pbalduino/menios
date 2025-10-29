#include <kernel/vfs.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <time.h>

#include <kernel/block_device.h>
#include <kernel/fs.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>

extern int fat32_open_adapter(void* fs_ctx, const char* path, int flags, file_t** out_file);
extern int fat32_unlink_adapter(void* fs_ctx, const char* path);
extern int fat32_mkdir_adapter(void* fs_ctx, const char* path, bool exclusive);
extern int fat32_rmdir_adapter(void* fs_ctx, const char* path);
extern int fat32_rename_adapter(void* fs_ctx, const char* old_path, const char* new_path);
extern int fat32_chmod_impl(void* fs_ctx, const char* path, mode_t mode);
extern int fat32_utimens_path(void* fs_ctx, const char* path, const struct timespec times[2]);

#define VFS_LOG_PATH_MAX 96

static const char* vfs_debug_path(const char* path, char* buffer, size_t buffer_len) {
  static const uintptr_t kernel_floor = 0xffff800000000000ull;
  static const char hex_digits[] = "0123456789abcdef";

  if(buffer == NULL || buffer_len == 0) {
    return "";
  }

  if(path == NULL) {
    static const char null_repr[] = "(null)";
    size_t copy = sizeof(null_repr);
    if(copy > buffer_len) {
      copy = buffer_len;
    }
    memcpy(buffer, null_repr, copy - 1);
    buffer[copy - 1] = '\0';
    return buffer;
  }

  uintptr_t addr = (uintptr_t)path;
  if(addr >= kernel_floor) {
    size_t copy_len = strnlen(path, buffer_len - 1);
    memcpy(buffer, path, copy_len);
    buffer[copy_len] = '\0';
    return buffer;
  }

  size_t pos = 0;
  static const char prefix[] = "(user:0x";
  for(size_t i = 0; i < sizeof(prefix) - 1 && pos < buffer_len - 1; i++) {
    buffer[pos++] = prefix[i];
  }

  bool started = false;
  for(int shift = (int)(sizeof(uintptr_t) * 8) - 4; shift >= 0 && pos < buffer_len - 1; shift -= 4) {
    char digit = hex_digits[(addr >> shift) & 0xF];
    if(!started) {
      if(digit == '0' && shift > 0) {
        continue;
      }
      started = true;
    }
    buffer[pos++] = digit;
  }
  if(!started && pos < buffer_len - 1) {
    buffer[pos++] = '0';
  }
  if(pos < buffer_len - 1) {
    buffer[pos++] = ')';
  }
  buffer[pos < buffer_len ? pos : buffer_len - 1] = '\0';
  return buffer;
}

typedef struct vfs_mount_entry_t {
  char                        path[128];
  size_t                      path_len;
  const vfs_fs_driver_t*      driver;
  void*                       fs_ctx;
  bool                        read_only;
  struct vfs_mount_entry_t*   next;
} vfs_mount_entry_t;

typedef struct vfs_file_buffer_t {
  uint8_t*               data;
  size_t                 size;
  size_t                 capacity;
  size_t                 offset;
  bool                   dirty;
  bool                   writable;
  bool                   streaming;
  bool                   size_known;
  bool                   info_valid;
  bool                   read_only;
  fs_path_info_t         info;
  const vfs_fs_driver_t* driver;
  void*                  fs_ctx;
  char                   relative[VFS_PATH_MAX];
} vfs_file_buffer_t;

static const file_ops_t vfs_file_ops;
static bool vfs_stream_refresh_size(vfs_file_buffer_t* ctx);
static bool vfs_resolve(const char* path,
                        const vfs_fs_driver_t** driver_out,
                        void** fs_ctx_out,
                        char* relative,
                        size_t relative_size,
                        bool* read_only_out);

static vfs_mount_entry_t* vfs_mounts = NULL;
static kmutex_t           vfs_lock;
static bool               vfs_initialized = false;

static bool vfs_canonicalize(const char* input, char* out, size_t out_size, size_t* out_len) {
  if(input == NULL || out == NULL || out_size == 0) {
    return false;
  }

  size_t len = strlen(input);
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

  const size_t max_segments = 64;
  size_t stack[max_segments];
  size_t depth = 0;
  size_t out_pos = 0;

  out[out_pos++] = '/';
  out[out_pos] = '\0';

  size_t i = 0;
  if(input[0] == '/') {
    i = 1;
    while(i < len && input[i] == '/') {
      i++;
    }
  }

  while(i < len) {
    size_t start = i;
    while(i < len && input[i] != '/') {
      i++;
    }
    size_t seg_len = i - start;
    while(i < len && input[i] == '/') {
      i++;
    }

    if(seg_len == 0) {
      continue;
    }

    if(seg_len == 1 && input[start] == '.') {
      continue;
    }

    if(seg_len == 2 && input[start] == '.' && input[start + 1] == '.') {
      if(depth > 0) {
        out_pos = stack[--depth];
        out[out_pos] = '\0';
      }
      continue;
    }

    if(depth >= max_segments) {
      return false;
    }

    if(out_pos > 1) {
      if(out_pos + 1 >= out_size) {
        return false;
      }
      out[out_pos++] = '/';
    }

    stack[depth++] = out_pos;
    if(out_pos + seg_len >= out_size) {
      return false;
    }
    memcpy(out + out_pos, input + start, seg_len);
    out_pos += seg_len;
    out[out_pos] = '\0';
  }

  if(out_pos > 1 && out[out_pos - 1] == '/') {
    out[--out_pos] = '\0';
  }

  if(out_len) {
    *out_len = out_pos;
  }
  return true;
}

bool vfs_build_absolute_path(const char* base, const char* path, char* out, size_t out_size) {
  if(path == NULL || out == NULL || out_size == 0) {
    return false;
  }

  char combined[VFS_PATH_MAX * 2];
  const char* effective_base = (base != NULL && base[0] != '\0') ? base : "/";

  if(path[0] == '/') {
    if(strlen(path) + 1 > sizeof(combined)) {
      return false;
    }
    for(size_t i = 0; path[i] != '\0'; ++i) {
      combined[i] = (path[i] == '\\') ? '/' : path[i];
      combined[i + 1] = '\0';
    }
  } else {
    size_t base_len = strlen(effective_base);
    size_t path_len = strlen(path);
    bool base_is_root = (base_len == 1 && effective_base[0] == '/');
    size_t needed = base_len + (base_is_root ? 0 : 1) + path_len + 1;
    if(needed > sizeof(combined)) {
      return false;
    }

    size_t pos = 0;
    for(size_t i = 0; i < base_len; ++i) {
      combined[pos++] = (effective_base[i] == '\\') ? '/' : effective_base[i];
    }
    if(!base_is_root && combined[pos - 1] != '/') {
      combined[pos++] = '/';
    }
    for(size_t i = 0; i < path_len; ++i) {
      char ch = path[i] == '\\' ? '/' : path[i];
      combined[pos++] = ch;
    }
    combined[pos] = '\0';
  }

  return vfs_canonicalize(combined, out, out_size, NULL);
}

static bool vfs_normalize_path(const char* path, char* out, size_t out_size, size_t* out_len) {
  if(!vfs_build_absolute_path("/", path, out, out_size)) {
    return false;
  }
  if(out_len) {
    *out_len = strlen(out);
  }
  return true;
}

bool vfs_path_info(const char* path, fs_path_info_t* out_info) {
  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[VFS_PATH_MAX];
  bool read_only = true;

  if(path == NULL || out_info == NULL) {
    return false;
  }

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), &read_only)) {
    return false;
  }

  if(driver->stat == NULL) {
    return false;
  }

  if(!driver->stat(fs_ctx, relative, out_info)) {
    if(relative[0] == '/' && relative[1] != '\0') {
      if(!driver->stat(fs_ctx, relative + 1, out_info)) {
        return false;
      }
    } else {
      return false;
    }
  }

  if(read_only) {
    out_info->is_read_only = true;
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

bool vfs_mount(const char* path, const vfs_fs_driver_t* driver, void* fs_ctx, bool read_only) {
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
  entry->read_only = read_only;
  entry->next = vfs_mounts;
  vfs_mounts = entry;
  kmutex_unlock(&vfs_lock);

  serial_printf("vfs: mounted %s\n", entry->path);
  return true;
}

bool vfs_mount_root(const vfs_fs_driver_t* driver, void* fs_ctx, bool read_only) {
  return vfs_mount("/", driver, fs_ctx, read_only);
}

static bool vfs_resolve(const char* path,
                        const vfs_fs_driver_t** driver_out,
                        void** fs_ctx_out,
                        char* relative,
                        size_t relative_size,
                        bool* read_only_out) {
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
  if(read_only_out) {
    *read_only_out = entry->read_only;
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
  char relative[VFS_PATH_MAX];
  bool read_only = true;

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), &read_only)) {
    return false;
  }

  if(driver->list == NULL) {
    return false;
  }

  bool ok = driver->list(fs_ctx, relative, iter, context);
  if(relative[0] == '/' && relative[1] == '\0') {
    kmutex_lock(&vfs_lock);
    for(vfs_mount_entry_t* entry = vfs_mounts; entry != NULL; entry = entry->next) {
      if(entry->path_len <= 1) {
        continue;
      }
      const char* mount_name = entry->path + 1;
      const char* slash = mount_name;
      while(*slash != '\0' && *slash != '/') {
        slash++;
      }
      if(*slash != '\0') {
        continue;
      }
      fs_dir_entry_t mount_entry;
      size_t name_len = strlen(mount_name);
      if(name_len >= sizeof(mount_entry.name)) {
        continue;
      }
      memcpy(mount_entry.name, mount_name, name_len + 1);
      mount_entry.is_directory = true;
      mount_entry.size = 0;
      iter(&mount_entry, context);
      ok = true;
    }
    kmutex_unlock(&vfs_lock);
  }

  return ok;
}

bool vfs_read(const char* path, size_t offset, void* buffer, size_t length, size_t* bytes_read) {
  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[VFS_PATH_MAX];

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), NULL)) {
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
  char relative[VFS_PATH_MAX];

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), NULL)) {
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
  if(ctx == NULL) {
    return -EINVAL;
  }

  if(ctx->streaming) {
    serial_printf("vfs_file_read: entry path=%s offset=%lu len=%lu\n",
                  ctx->relative,
                  (unsigned long)ctx->offset,
                  (unsigned long)length);
    if(ctx->driver == NULL || ctx->driver->read == NULL) {
      return -ENOSYS;
    }
    size_t bytes = 0;
    if(!ctx->driver->read(ctx->fs_ctx, ctx->relative, ctx->offset, buffer, length, &bytes)) {
      serial_printf("vfs_file_read: driver read failed path=%s offset=%lu len=%lu\n",
                    ctx->relative,
                    (unsigned long)ctx->offset,
                    (unsigned long)length);
      return -EIO;
    }
    ctx->offset += bytes;
    if(ctx->size_known && ctx->offset > ctx->size) {
      ctx->size = ctx->offset;
    }
    serial_printf("vfs_file_read: success path=%s read=%lu new_offset=%lu\n",
                  ctx->relative,
                  (unsigned long)bytes,
                  (unsigned long)ctx->offset);
    return (int64_t)bytes;
  }

  if(ctx->data == NULL) {
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

static int64_t vfs_file_write_impl(file_t* file, const void* buffer, size_t length) {
  if(file == NULL || buffer == NULL) {
    return -EINVAL;
  }

  vfs_file_buffer_t* ctx = (vfs_file_buffer_t*)file->private_data;
  if(ctx == NULL) {
    return -EINVAL;
  }

  if(!ctx->writable) {
    return -EBADF;
  }

  if(length == 0) {
    return 0;
  }

  if(ctx->streaming) {
    serial_printf("vfs_file_write: entry path=%s offset=%lu len=%lu\n",
                  ctx->relative,
                  (unsigned long)ctx->offset,
                  (unsigned long)length);
    if(ctx->driver == NULL || ctx->driver->write == NULL) {
      return -ENOSYS;
    }
    size_t written = 0;
    if(!ctx->driver->write(ctx->fs_ctx, ctx->relative, ctx->offset, buffer, length, &written)) {
      serial_printf("vfs_file_write: driver write failed path=%s offset=%lu len=%lu\n",
                    ctx->relative,
                    (unsigned long)ctx->offset,
                    (unsigned long)length);
      return -EIO;
    }
    ctx->offset += written;
    if(ctx->size_known) {
      if(ctx->offset > ctx->size) {
        ctx->size = ctx->offset;
      }
    } else {
      ctx->size = ctx->offset;
      ctx->size_known = true;
    }
    if(written != length) {
      serial_printf("vfs_file_write: short write path=%s requested=%lu written=%lu new_offset=%lu\n",
                    ctx->relative,
                    (unsigned long)length,
                    (unsigned long)written,
                    (unsigned long)ctx->offset);
    } else {
      serial_printf("vfs_file_write: success path=%s written=%lu new_offset=%lu\n",
                    ctx->relative,
                    (unsigned long)written,
                    (unsigned long)ctx->offset);
    }
    return (int64_t)written;
  }

  if(ctx->data == NULL) {
    return -EINVAL;
  }

  size_t required = ctx->offset + length;
  size_t new_capacity = ctx->capacity ? ctx->capacity : 1;
  while(new_capacity < required) {
    new_capacity *= 2;
  }

  if(new_capacity != ctx->capacity) {
    uint8_t* resized = krealloc(ctx->data, new_capacity);
    if(resized == NULL) {
      return -ENOMEM;
    }
    if(new_capacity > ctx->capacity) {
      memset(resized + ctx->capacity, 0, new_capacity - ctx->capacity);
    }
    ctx->data = resized;
    ctx->capacity = new_capacity;
  }

  if(ctx->offset > ctx->size) {
    memset(ctx->data + ctx->size, 0, ctx->offset - ctx->size);
  }

  memcpy(ctx->data + ctx->offset, buffer, length);
  ctx->offset += length;
  if(ctx->offset > ctx->size) {
    ctx->size = ctx->offset;
  }
  ctx->dirty = true;
  ctx->info_valid = false;
  return (int64_t)length;
}

static int vfs_file_stat_impl(file_t* file, struct stat* out_stat) {
  if(file == NULL || out_stat == NULL) {
    return -EINVAL;
  }

  vfs_file_buffer_t* ctx = (vfs_file_buffer_t*)file->private_data;
  if(ctx == NULL) {
    return -EINVAL;
  }

  fs_path_info_t info;
  if(ctx->info_valid) {
    info = ctx->info;
  } else if(ctx->driver && ctx->driver->stat &&
            ctx->driver->stat(ctx->fs_ctx, ctx->relative, &info)) {
    ctx->info = info;
    ctx->info_valid = true;
  } else {
    memset(&info, 0, sizeof(info));
    info.is_directory = false;
    info.is_read_only = !ctx->writable;
    info.block_size = ctx->info.block_size ? ctx->info.block_size : 512;
    info.size = ctx->size_known ? ctx->size : 0;
    info.inode = ctx->info.inode;
  }

  if(ctx->size_known && info.size < ctx->size) {
    info.size = ctx->size;
  }

  if(info.block_size == 0) {
    info.block_size = 512;
  }

  fs_path_info_to_stat(&info, out_stat);
  return 0;
}

static int vfs_file_chmod_impl(file_t* file, mode_t mode) {
  if(file == NULL) {
    return -EINVAL;
  }

  vfs_file_buffer_t* ctx = (vfs_file_buffer_t*)file->private_data;
  if(ctx == NULL) {
    return -EINVAL;
  }

  if(ctx->driver == NULL || ctx->driver->chmod == NULL) {
    return -ENOSYS;
  }

  if(ctx->read_only) {
    return -EROFS;
  }

  int rc = ctx->driver->chmod(ctx->fs_ctx, ctx->relative, mode);
  if(rc == 0) {
    ctx->info_valid = false;
  }
  return rc;
}

static int vfs_file_utimens_impl(file_t* file, const struct timespec times[2]) {
  if(file == NULL) {
    return -EINVAL;
  }

  vfs_file_buffer_t* ctx = (vfs_file_buffer_t*)file->private_data;
  if(ctx == NULL) {
    return -EINVAL;
  }

  if(ctx->driver == NULL || ctx->driver->utimens == NULL) {
    return -ENOSYS;
  }

  if(ctx->read_only) {
    return -EROFS;
  }

  int rc = ctx->driver->utimens(ctx->fs_ctx, ctx->relative, times);
  if(rc == 0) {
    ctx->info_valid = false;
  }
  return rc;
}

static int vfs_file_close_impl(file_t* file) {
  if(file == NULL) {
    return 0;
  }
  vfs_file_buffer_t* ctx = (vfs_file_buffer_t*)file->private_data;
  if(ctx == NULL) {
    return 0;
  }

  int rc = 0;
  if(!ctx->streaming && ctx->dirty && ctx->writable && ctx->driver && ctx->driver->write_all) {
    if(!ctx->driver->write_all(ctx->fs_ctx, ctx->relative, ctx->data, ctx->size)) {
      rc = -EIO;
    }
  }

  if(ctx->data) {
    kfree(ctx->data);
  }
  kfree(ctx);
  file->private_data = NULL;
  return rc;
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
      if(ctx->streaming && !ctx->size_known) {
        if(!vfs_stream_refresh_size(ctx)) {
          return -ENOSYS;
        }
      }
      base = (int64_t)ctx->size;
      break;
    default:
      return -EINVAL;
  }

  int64_t new_offset = base + offset;
  if(new_offset < 0) {
    return -EINVAL;
  }

  if(!ctx->streaming) {
    if((uint64_t)new_offset > ctx->size) {
      return -EINVAL;
    }
  } else if(ctx->size_known && (uint64_t)new_offset > ctx->size) {
    if(ctx->writable) {
      ctx->size = (size_t)new_offset;
      ctx->size_known = true;
    } else {
      return -EINVAL;
    }
  }

  ctx->offset = (size_t)new_offset;
  return new_offset;
}

static const file_ops_t vfs_file_ops = {
  .read = vfs_file_read_impl,
  .write = vfs_file_write_impl,
  .close = vfs_file_close_impl,
  .seek = vfs_file_seek_impl,
  .ioctl = NULL,
  .mmap = NULL,
  .stat = vfs_file_stat_impl,
  .chmod = vfs_file_chmod_impl,
  .utimens = vfs_file_utimens_impl,
};

static int vfs_open_buffered(const vfs_fs_driver_t* driver,
                             void* fs_ctx,
                             const char* relative_path,
                             int flags,
                             bool read_only,
                             file_t** out_file) {
  bool writable = false;
  int accmode = flags & O_ACCMODE;
  if(accmode == O_WRONLY || accmode == O_RDWR) {
    writable = true;
  }

  bool append_requested = (flags & O_APPEND) != 0;
  bool metadata_mutation = (flags & (O_CREAT | O_TRUNC)) != 0;
  bool write_requested = writable || append_requested;
  if((write_requested || metadata_mutation) && read_only) {
    return -EROFS;
  }

  bool streaming = false;
  if(write_requested || metadata_mutation) {
    if(driver->write == NULL) {
      return -ENOSYS;
    }
    streaming = true;
  }

  if(!streaming && driver->read_all == NULL) {
    if(driver->read != NULL) {
      streaming = true;
    } else {
      return -ENOSYS;
    }
  }

  if(flags & O_CREAT) {
    if(driver->create_file == NULL) {
      return -ENOSYS;
    }
    bool exclusive = (flags & O_EXCL) != 0;
    if(!driver->create_file(fs_ctx, relative_path, exclusive)) {
      serial_printf("vfs_open: create failed path=%s flags=0x%x\n", relative_path, flags);
      return exclusive ? -EEXIST : -EIO;
    }
    serial_printf("vfs_open: create succeeded path=%s\n", relative_path);
  } else if(flags & O_EXCL) {
    return -EINVAL;
  }

  if(flags & O_TRUNC) {
    if(driver->truncate_file == NULL) {
      return -ENOSYS;
    }
    if(!driver->truncate_file(fs_ctx, relative_path)) {
      return -EIO;
    }
  }

  size_t file_size = 0;
  bool size_known = false;
  bool info_valid = false;
  fs_path_info_t path_info;
  memset(&path_info, 0, sizeof(path_info));

  if(streaming) {
    if((flags & O_TRUNC) || (flags & O_CREAT)) {
      file_size = 0;
      size_known = true;
      info_valid = true;
      path_info.is_directory = false;
      path_info.is_read_only = !writable;
      path_info.block_size = 512;
      path_info.size = 0;
      path_info.inode = 0;
    } else if(driver->stat && driver->stat(fs_ctx, relative_path, &path_info)) {
      file_size = (size_t)path_info.size;
      size_known = true;
      info_valid = true;
    } else if(driver->read_all != NULL && !metadata_mutation) {
      void* tmp = NULL;
      size_t tmp_size = 0;
      if(driver->read_all(fs_ctx, relative_path, &tmp, &tmp_size)) {
        file_size = tmp_size;
        size_known = true;
      }
      if(tmp) {
        kfree(tmp);
      }
    }
  }

  if(streaming) {
    vfs_file_buffer_t* ctx = kmalloc(sizeof(vfs_file_buffer_t));
    if(ctx == NULL) {
      return -ENOMEM;
    }

    ctx->data = NULL;
    ctx->size = size_known ? file_size : 0;
    ctx->capacity = 0;
    ctx->offset = 0;
    ctx->dirty = false;
    ctx->writable = writable;
  	ctx->streaming = true;
    ctx->size_known = size_known;
    ctx->info_valid = info_valid;
    if(info_valid) {
      ctx->info = path_info;
    } else {
      memset(&ctx->info, 0, sizeof(ctx->info));
    }
    ctx->driver = driver;
    ctx->fs_ctx = fs_ctx;
    strncpy(ctx->relative, relative_path, sizeof(ctx->relative) - 1);
    ctx->relative[sizeof(ctx->relative) - 1] = '\0';
    ctx->read_only = read_only;

    if(append_requested) {
      if(!ctx->size_known && !vfs_stream_refresh_size(ctx) && driver->read_all != NULL) {
        void* tmp = NULL;
        size_t tmp_size = 0;
        if(driver->read_all(fs_ctx, relative_path, &tmp, &tmp_size)) {
          ctx->size = tmp_size;
          ctx->size_known = true;
        }
        if(tmp) {
          kfree(tmp);
        }
      }
      if(ctx->size_known) {
        ctx->offset = ctx->size;
      }
    }

    uint32_t mode = FILE_MODE_READ;
    if(writable) {
      mode |= FILE_MODE_WRITE;
    }

    file_t* file = file_create(&vfs_file_ops, ctx, mode);
    if(file == NULL) {
      kfree(ctx);
      return -ENOMEM;
    }

    *out_file = file;
    return 0;
  }

  if(driver->read_all == NULL) {
    return -ENOSYS;
  }

  void* data = NULL;
  size_t size = 0;
  if(!driver->read_all(fs_ctx, relative_path, &data, &size)) {
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
  ctx->capacity = size;
  ctx->offset = 0;
  ctx->dirty = false;
  ctx->writable = writable;
  ctx->streaming = false;
  ctx->size_known = true;
  if(driver->stat && driver->stat(fs_ctx, relative_path, &path_info)) {
    ctx->info = path_info;
    ctx->info_valid = true;
  } else {
    ctx->info_valid = false;
    memset(&ctx->info, 0, sizeof(ctx->info));
  }
  ctx->driver = driver;
  ctx->fs_ctx = fs_ctx;
  strncpy(ctx->relative, relative_path, sizeof(ctx->relative) - 1);
  ctx->relative[sizeof(ctx->relative) - 1] = '\0';
  ctx->read_only = read_only;

  if(append_requested) {
    ctx->offset = ctx->size;
  }

  uint32_t mode = FILE_MODE_READ;
  if(writable) {
    mode |= FILE_MODE_WRITE;
  }

  file_t* file = file_create(&vfs_file_ops, ctx, mode);
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

  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[VFS_PATH_MAX];

  bool read_only = true;
  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), &read_only)) {
    serial_printf("vfs_open: resolve failed path=%s\n", path ? path : "(null)");
    return -ENOENT;
  }

  if(driver->open) {
    int rc = driver->open(fs_ctx, relative, flags, out_file);
    if(rc != -ENOSYS) {
      if(rc < 0) {
        serial_printf("vfs_open: driver open failed path=%s relative=%s flags=0x%x rc=%d read_only=%s\n",
                      path,
                      relative,
                      flags,
                      rc,
                      read_only ? "yes" : "no");
      }
      return rc;
    }
  }

  if(flags & O_DIRECTORY) {
    return -ENOSYS;
  }

  int buffered = vfs_open_buffered(driver, fs_ctx, relative, flags, read_only, out_file);
  if(buffered < 0) {
    serial_printf("vfs_open: buffered failed path=%s relative=%s flags=0x%x rc=%d read_only=%s\n",
                  path,
                  relative,
                  flags,
                  buffered,
                  read_only ? "yes" : "no");
  }
  return buffered;
}

int vfs_unlink(const char* path) {
  if(path == NULL) {
    return -EINVAL;
  }

  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[VFS_PATH_MAX];
  bool read_only = true;

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), &read_only)) {
    return -ENOENT;
  }

  if(read_only) {
    return -EROFS;
  }

  if(driver->unlink == NULL) {
    return -ENOSYS;
  }

  return driver->unlink(fs_ctx, relative);
}

int vfs_rmdir(const char* path) {
  if(path == NULL) {
    return -EINVAL;
  }

  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[VFS_PATH_MAX];
  bool read_only = true;

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), &read_only)) {
    return -ENOENT;
  }

  if(read_only) {
    return -EROFS;
  }

  if(driver->rmdir == NULL) {
    return -ENOSYS;
  }

  return driver->rmdir(fs_ctx, relative);
}

int vfs_mkdir(const char* path) {
  if(path == NULL) {
    return -EINVAL;
  }

  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[VFS_PATH_MAX];
  bool read_only = true;

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), &read_only)) {
    return -ENOENT;
  }

  if(read_only) {
    return -EROFS;
  }

  if(driver->mkdir == NULL) {
    return -ENOSYS;
  }

  if(vfs_path_is_directory(path)) {
    return -EEXIST;
  }

  file_t* existing = NULL;
  int open_rc = vfs_open(path, O_RDONLY, &existing);
  if(open_rc >= 0) {
    if(existing != NULL) {
      file_unref(existing);
    }
    return -EEXIST;
  }

  if(existing != NULL) {
    file_unref(existing);
  }

  return driver->mkdir(fs_ctx, relative, true);
}

int vfs_rename(const char* old_path, const char* new_path) {
  if(old_path == NULL || new_path == NULL) {
    return -EINVAL;
  }

  if(strcmp(old_path, new_path) == 0) {
    return 0;
  }

  const vfs_fs_driver_t* old_driver = NULL;
  void* old_ctx = NULL;
  char old_relative[VFS_PATH_MAX];
  bool old_read_only = true;

  if(!vfs_resolve(old_path, &old_driver, &old_ctx, old_relative, sizeof(old_relative), &old_read_only)) {
    return -ENOENT;
  }

  const vfs_fs_driver_t* new_driver = NULL;
  void* new_ctx = NULL;
  char new_relative[VFS_PATH_MAX];
  bool new_read_only = true;

  if(!vfs_resolve(new_path, &new_driver, &new_ctx, new_relative, sizeof(new_relative), &new_read_only)) {
    return -ENOENT;
  }

  if(old_driver != new_driver || old_ctx != new_ctx) {
    return -EXDEV;
  }

  if(old_read_only || new_read_only) {
    return -EROFS;
  }

  if(old_driver->rename == NULL) {
    return -ENOSYS;
  }

  return old_driver->rename(old_ctx, old_relative, new_relative);
}

int vfs_chmod(const char* path, mode_t mode) {
  if(path == NULL) {
    return -EINVAL;
  }

  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[VFS_PATH_MAX];
  bool read_only = true;

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), &read_only)) {
    return -ENOENT;
  }

  if(read_only) {
    return -EROFS;
  }

  if(driver == NULL || driver->chmod == NULL) {
    return -ENOSYS;
  }

  return driver->chmod(fs_ctx, relative, mode);
}

int vfs_utimens(const char* path, const struct timespec times[2]) {
  if(path == NULL) {
    return -EINVAL;
  }

  const vfs_fs_driver_t* driver = NULL;
  void* fs_ctx = NULL;
  char relative[VFS_PATH_MAX];
  bool read_only = true;

  if(!vfs_resolve(path, &driver, &fs_ctx, relative, sizeof(relative), &read_only)) {
    return -ENOENT;
  }

  if(read_only) {
    return -EROFS;
  }

  if(driver == NULL || driver->utimens == NULL) {
    return -ENOSYS;
  }

  return driver->utimens(fs_ctx, relative, times);
}

static bool vfs_directory_probe_iter(const fs_dir_entry_t* entry, void* context) {
  (void)entry;
  (void)context;
  return true;
}

bool vfs_path_is_directory(const char* path) {
  if(path == NULL) {
    return false;
  }
  fs_path_info_t info;
  if(vfs_path_info(path, &info)) {
    return info.is_directory;
  }
  return vfs_list(path, vfs_directory_probe_iter, NULL);
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

static bool fat32_write_adapter(void* fs_ctx,
                                const char* path,
                                size_t offset,
                                const void* buffer,
                                size_t length,
                                size_t* bytes_written) {
  const fs_mount_t* mount = (const fs_mount_t*)fs_ctx;
  serial_printf("fat32_write_adapter: path=%s offset=%llu len=%llu\n",
                path ? path : "(null)",
                (unsigned long long)offset,
                (unsigned long long)length);
  bool ok = fs_file_write(mount, path, offset, buffer, length, bytes_written);
  if(!ok && path != NULL && path[0] == '/' && path[1] != '\0') {
    serial_printf("fat32_write_adapter: retry without leading slash path=%s\n", path);
    ok = fs_file_write(mount, path + 1, offset, buffer, length, bytes_written);
  }
  serial_printf("fat32_write_adapter: result=%s bytes_written=%llu\n",
                ok ? "ok" : "fail",
                (unsigned long long)(bytes_written ? *bytes_written : 0u));
  return ok;
}

static bool fat32_write_all_adapter(void* fs_ctx, const char* path, const void* buffer, size_t size) {
  const fs_mount_t* mount = (const fs_mount_t*)fs_ctx;
  bool ok = fs_file_write_all(mount, path, buffer, size);
  if(!ok && path != NULL && path[0] == '/' && path[1] != '\0') {
    ok = fs_file_write_all(mount, path + 1, buffer, size);
  }
  return ok;
}

static bool fat32_stat_adapter(void* fs_ctx, const char* path, fs_path_info_t* out_info) {
  const fs_mount_t* mount = (const fs_mount_t*)fs_ctx;
  if(mount == NULL || out_info == NULL) {
    return false;
  }
  if(fs_path_info(mount, path, out_info)) {
    return true;
  }
  if(path != NULL && path[0] == '/' && path[1] != '\0') {
    return fs_path_info(mount, path + 1, out_info);
  }
  return false;
}

static bool fat32_create_file_adapter(void* fs_ctx, const char* path, bool exclusive) {
  const fs_mount_t* mount = (const fs_mount_t*)fs_ctx;
  if(mount == NULL) {
    return false;
  }
  char path_repr[VFS_LOG_PATH_MAX];
  const char* printable_path = vfs_debug_path(path, path_repr, sizeof(path_repr));
  serial_printf("fat32_create_file_adapter: path=%s ptr=%p exclusive=%s\n",
                printable_path,
                (void*)path,
                exclusive ? "yes" : "no");
  if(fs_file_create(mount, path, exclusive)) {
    serial_printf("fat32_create_file_adapter: created %s\n", printable_path);
    return true;
  }
  serial_printf("fat32_create_file_adapter: failed path=%s\n", printable_path);
  if(path != NULL && path[0] == '/' && path[1] != '\0') {
    return fs_file_create(mount, path + 1, exclusive);
  }
  return false;
}

static bool fat32_truncate_file_adapter(void* fs_ctx, const char* path) {
  const fs_mount_t* mount = (const fs_mount_t*)fs_ctx;
  if(mount == NULL) {
    return false;
  }
  if(fs_file_truncate(mount, path)) {
    return true;
  }
  if(path != NULL && path[0] == '/' && path[1] != '\0') {
    return fs_file_truncate(mount, path + 1);
  }
  return false;
}

static void fat32_destroy_adapter(void* fs_ctx) {
  fs_unmount((fs_mount_t*)fs_ctx);
}

static const vfs_fs_driver_t fat32_driver = {
  .list = fat32_list_adapter,
  .read = fat32_read_adapter,
  .read_all = fat32_read_all_adapter,
  .write = fat32_write_adapter,
  .write_all = fat32_write_all_adapter,
  .create_file = fat32_create_file_adapter,
  .truncate_file = fat32_truncate_file_adapter,
  .stat = fat32_stat_adapter,
  .open = fat32_open_adapter,
  .unlink = fat32_unlink_adapter,
  .mkdir = fat32_mkdir_adapter,
  .rmdir = fat32_rmdir_adapter,
  .rename = fat32_rename_adapter,
  .chmod = fat32_chmod_impl,
  .utimens = fat32_utimens_path,
  .destroy = fat32_destroy_adapter,
};

static bool vfs_stream_refresh_size(vfs_file_buffer_t* ctx) {
  if(ctx == NULL || !ctx->streaming || ctx->driver == NULL || ctx->driver->stat == NULL) {
    return false;
  }

  fs_path_info_t info;
  if(ctx->driver->stat(ctx->fs_ctx, ctx->relative, &info)) {
    ctx->size = (size_t)info.size;
    ctx->size_known = true;
    ctx->info = info;
    ctx->info_valid = true;
    return true;
  }
  return false;
}

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

  if(!vfs_mount_root(&fat32_driver, mount, false)) {
    fs_unmount(mount);
    return false;
  }

  mounted = true;
  return true;
}
