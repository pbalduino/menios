#include <kernel/fs/procfs/procfs.h>

#include <errno.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <kernel/console.h>
#include <kernel/file.h>
#include <kernel/fs/core.h>
#include <kernel/fs/devfs/devfs.h>
#include <kernel/heap.h>
#include <kernel/pmm.h>
#include <kernel/tsc.h>
#include <kernel/fs/vfs/vfs.h>

#define PROCFS_MAX_NAME 64

typedef bool (*procfs_generate_fn)(char** out_buffer, size_t* out_size);

typedef struct procfs_entry_t {
  const char*        name;
  procfs_generate_fn generate;
} procfs_entry_t;

typedef struct procfs_file_state_t {
  const procfs_entry_t* entry;
  char*  buffer;
  size_t size;
  size_t offset;
  struct timespec atime;
  struct timespec mtime;
  struct timespec ctime;
} procfs_file_state_t;

static bool procfs_generate_meminfo(char** out_buffer, size_t* out_size);
static bool procfs_generate_devices(char** out_buffer, size_t* out_size);

static const procfs_entry_t procfs_entries[] = {
  { "meminfo", procfs_generate_meminfo },
  { "devices", procfs_generate_devices },
};

static size_t procfs_entry_count(void) {
  return sizeof(procfs_entries) / sizeof(procfs_entries[0]);
}

static const procfs_entry_t* procfs_find_entry(const char* path) {
  if(path == NULL) {
    return NULL;
  }
  while(*path == '/') {
    path++;
  }
  if(*path == '\0') {
    return NULL;
  }
  for(size_t i = 0; i < procfs_entry_count(); ++i) {
    if(strcmp(path, procfs_entries[i].name) == 0) {
      return &procfs_entries[i];
    }
  }
  return NULL;
}

static bool procfs_list(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context) {
  (void)fs_ctx;
  if(path != NULL && path[0] != '\0' && strcmp(path, "/") != 0) {
    return false;
  }
  for(size_t i = 0; i < procfs_entry_count(); ++i) {
    vfs_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, procfs_entries[i].name, sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.is_directory = false;
    entry.size = 0;
    if(!iter(&entry, context)) {
      break;
    }
  }
  return true;
}

static bool procfs_generate(const char* path, char** out_buffer, size_t* out_size) {
  const procfs_entry_t* entry = procfs_find_entry(path);
  if(entry == NULL || entry->generate == NULL) {
    return false;
  }
  return entry->generate(out_buffer, out_size);
}

static void procfs_fill_file_info(const procfs_entry_t* entry,
                                  size_t size,
                                  fs_path_info_t* out_info) {
  memset(out_info, 0, sizeof(*out_info));
  out_info->block_size = 4096;
  out_info->inode = entry ? (uint64_t)(entry - procfs_entries + 1) : 0;
  out_info->is_read_only = true;
  out_info->has_mode = true;
  if(entry == NULL) {
    out_info->is_directory = true;
    out_info->mode = S_IFDIR | 0555;
  } else {
    out_info->is_directory = false;
    out_info->size = size;
    out_info->mode = S_IFREG | 0444;
  }
  out_info->has_times = true;
  uint64_t usec = unix_time_us();
  struct timespec now;
  now.tv_sec = (time_t)(usec / 1000000ull);
  now.tv_nsec = (long)((usec % 1000000ull) * 1000ull);
  out_info->atime = now;
  out_info->mtime = now;
  out_info->ctime = now;
}

static bool procfs_stat(void* fs_ctx, const char* path, fs_path_info_t* out_info) {
  (void)fs_ctx;
  if(out_info == NULL || path == NULL) {
    return false;
  }

  if(path[0] == '\0' || strcmp(path, "/") == 0) {
    procfs_fill_file_info(NULL, 0, out_info);
    return true;
  }

  const procfs_entry_t* entry = procfs_find_entry(path);
  if(entry == NULL) {
    return false;
  }

  char* buffer = NULL;
  size_t size = 0;
  if(!entry->generate(&buffer, &size)) {
    return false;
  }
  if(buffer != NULL) {
    kfree(buffer);
  }

  procfs_fill_file_info(entry, size, out_info);
  return true;
}

static bool procfs_read(void* fs_ctx,
                        const char* path,
                        size_t offset,
                        void* buffer,
                        size_t length,
                        size_t* bytes_read) {
  (void)fs_ctx;
  if(buffer == NULL || bytes_read == NULL) {
    return false;
  }
  char* data = NULL;
  size_t size = 0;
  if(!procfs_generate(path, &data, &size)) {
    return false;
  }
  if(offset >= size) {
    *bytes_read = 0;
    kfree(data);
    return true;
  }
  size_t to_copy = length;
  if(offset + to_copy > size) {
    to_copy = size - offset;
  }
  memcpy(buffer, data + offset, to_copy);
  *bytes_read = to_copy;
  kfree(data);
  return true;
}

static bool procfs_read_all(void* fs_ctx, const char* path, void** out_buffer, size_t* out_size) {
  (void)fs_ctx;
  if(out_buffer == NULL || out_size == NULL) {
    return false;
  }
  return procfs_generate(path, (char**)out_buffer, out_size);
}

static int64_t procfs_file_read(file_t* file, void* buffer, size_t length) {
  if(file == NULL || buffer == NULL || length == 0) {
    return -EINVAL;
  }
  procfs_file_state_t* state = (procfs_file_state_t*)file->private_data;
  if(state == NULL || state->buffer == NULL) {
    return -EIO;
  }
  if(state->offset >= state->size) {
    return 0;
  }
  size_t to_copy = length;
  if(state->offset + to_copy > state->size) {
    to_copy = state->size - state->offset;
  }
  memcpy(buffer, state->buffer + state->offset, to_copy);
  state->offset += to_copy;
  if(to_copy > 0) {
    uint64_t usec = unix_time_us();
    state->atime.tv_sec = (time_t)(usec / 1000000ull);
    state->atime.tv_nsec = (long)((usec % 1000000ull) * 1000ull);
  }
  return (int64_t)to_copy;
}

static int procfs_file_close(file_t* file) {
  if(file == NULL) {
    return 0;
  }
  procfs_file_state_t* state = (procfs_file_state_t*)file->private_data;
  if(state != NULL) {
    if(state->buffer != NULL) {
      kfree(state->buffer);
    }
    kfree(state);
    file->private_data = NULL;
  }
  return 0;
}

static int procfs_file_stat(file_t* file, struct stat* out_stat) {
  if(file == NULL || out_stat == NULL) {
    return -EINVAL;
  }
  procfs_file_state_t* state = (procfs_file_state_t*)file->private_data;
  if(state == NULL || state->entry == NULL) {
    return -EINVAL;
  }
  fs_path_info_t info;
  procfs_fill_file_info(state->entry, state->size, &info);
  info.atime = state->atime;
  info.mtime = state->mtime;
  info.ctime = state->ctime;
  fs_path_info_to_stat(&info, out_stat);
  return 0;
}

static const file_ops_t procfs_file_ops = {
  .read = procfs_file_read,
  .write = NULL,
  .close = procfs_file_close,
  .seek = NULL,
  .ioctl = NULL,
  .mmap = NULL,
  .stat = procfs_file_stat,
  .chmod = NULL,
  .utimens = NULL,
};

static int procfs_open(void* fs_ctx, const char* path, int flags, file_t** out_file) {
  (void)fs_ctx;
  if(out_file == NULL) {
    return -EINVAL;
  }
  if((flags & O_ACCMODE) != O_RDONLY) {
    return -EACCES;
  }

  const procfs_entry_t* entry = procfs_find_entry(path);
  if(entry == NULL) {
    return -ENOENT;
  }

  char* buffer = NULL;
  size_t size = 0;
  if(!procfs_generate(path, &buffer, &size)) {
    return -ENOENT;
  }

  procfs_file_state_t* state = kmalloc(sizeof(procfs_file_state_t));
  if(state == NULL) {
    kfree(buffer);
    return -ENOMEM;
  }
  state->entry = entry;
  state->buffer = buffer;
  state->size = size;
  state->offset = 0;
  uint64_t usec = unix_time_us();
  time_t sec = (time_t)(usec / 1000000ull);
  long nsec = (long)((usec % 1000000ull) * 1000ull);
  state->atime.tv_sec = state->mtime.tv_sec = state->ctime.tv_sec = sec;
  state->atime.tv_nsec = state->mtime.tv_nsec = state->ctime.tv_nsec = nsec;

  file_t* handle = file_create(&procfs_file_ops, state, FILE_MODE_READ);
  if(handle == NULL) {
    kfree(buffer);
    kfree(state);
    return -ENOMEM;
  }

  *out_file = handle;
  return 0;
}

static int procfs_unlink(void* fs_ctx, const char* path) {
  (void)fs_ctx;
  (void)path;
  return -EACCES;
}

static bool procfs_write(void* fs_ctx,
                         const char* path,
                         size_t offset,
                         const void* buffer,
                         size_t length,
                         size_t* bytes_written) {
  (void)fs_ctx;
  (void)path;
  (void)offset;
  (void)buffer;
  (void)length;
  (void)bytes_written;
  return false;
}

static bool procfs_write_all(void* fs_ctx, const char* path, const void* buffer, size_t size) {
  (void)fs_ctx;
  (void)path;
  (void)buffer;
  (void)size;
  return false;
}

static void procfs_destroy(void* fs_ctx) {
  (void)fs_ctx;
}

typedef struct {
  char*  buffer;
  size_t length;
  size_t capacity;
  bool   truncated;
} procfs_device_list_ctx_t;

static void procfs_devices_iterate_cb(const char_device_t* device, void* data) {
  procfs_device_list_ctx_t* ctx = (procfs_device_list_ctx_t*)data;
  if(device == NULL || ctx == NULL || ctx->truncated) {
    return;
  }

  char line[128];
  int line_len = vprintk(line, " %u %s\n", MAJOR(device->dev), device->name);
  if(line_len <= 0) {
    return;
  }

  size_t needed = ctx->length + (size_t)line_len;
  if(needed >= ctx->capacity) {
    size_t new_capacity = ctx->capacity * 2;
    while(new_capacity <= needed) {
      new_capacity *= 2;
    }
    char* new_buffer = krealloc(ctx->buffer, new_capacity);
    if(new_buffer == NULL) {
      ctx->truncated = true;
      return;
    }
    ctx->buffer = new_buffer;
    ctx->capacity = new_capacity;
  }

  memcpy(ctx->buffer + ctx->length, line, (size_t)line_len);
  ctx->length += (size_t)line_len;
}

static bool procfs_generate_devices(char** out_buffer, size_t* out_size) {
  if(out_buffer == NULL || out_size == NULL) {
    return false;
  }

  size_t capacity = 256;
  char* buffer = kmalloc(capacity);
  if(buffer == NULL) {
    return false;
  }

  int header_len = vprintk(buffer, "Character devices:\n");
  if(header_len < 0) {
    kfree(buffer);
    return false;
  }

  procfs_device_list_ctx_t ctx = {
    .buffer = buffer,
    .length = (size_t)header_len,
    .capacity = capacity,
    .truncated = false,
  };

  char_device_iterate(procfs_devices_iterate_cb, &ctx);

  if(ctx.truncated) {
    kfree(ctx.buffer);
    return false;
  }

  *out_buffer = ctx.buffer;
  *out_size = ctx.length;
  return true;
}

static const vfs_fs_driver_t procfs_driver = {
  .list = procfs_list,
  .read = procfs_read,
  .read_all = procfs_read_all,
  .write = procfs_write,
  .write_all = procfs_write_all,
  .create_file = NULL,
  .truncate_file = NULL,
  .stat = procfs_stat,
  .open = procfs_open,
  .unlink = procfs_unlink,
  .mkdir = NULL,
  .rmdir = NULL,
  .rename = NULL,
  .mknod = NULL,
  .chmod = NULL,
  .utimens = NULL,
  .destroy = procfs_destroy,
};

static bool procfs_generate_meminfo(char** out_buffer, size_t* out_size) {
  if(out_buffer == NULL || out_size == NULL) {
    return false;
  }

  pmm_stats_t stats;
  pmm_get_stats(&stats);

  uint64_t usable_bytes = (uint64_t)stats.usable_pages * PAGE_SIZE;
  uint64_t free_bytes = (uint64_t)stats.free_pages * PAGE_SIZE;
  uint64_t used_bytes = usable_bytes - free_bytes;

  char* buffer = kmalloc(256);
  if(buffer == NULL) {
    return false;
  }

  int written = vprintk(buffer,
                        "usable_bytes: %llu\n"
                        "free_bytes: %llu\n"
                        "used_bytes: %llu\n"
                        "usable_pages: %zu\n"
                        "free_pages: %zu\n",
                        (unsigned long long)usable_bytes,
                        (unsigned long long)free_bytes,
                        (unsigned long long)used_bytes,
                        stats.usable_pages,
                        stats.free_pages);
  if(written < 0 || written >= 256) {
    kfree(buffer);
    return false;
  }

  *out_buffer = buffer;
  *out_size = (size_t)written;
  return true;
}

bool procfs_mount(void) {
  return vfs_mount("/proc", &procfs_driver, NULL, true);
}
