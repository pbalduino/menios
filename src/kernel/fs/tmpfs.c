#include <kernel/tmpfs.h>

#include <errno.h>
#include <string.h>
#include <sys/fcntl.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/file.h>
#include <kernel/vfs.h>
#include <kernel/serial.h>

typedef struct tmpfs_node_t {
  char               name[128];
  uint8_t*           data;
  size_t             size;
  size_t             capacity;
  struct tmpfs_node_t* next;
  bool               deleted;
  uint32_t           refcount;
} tmpfs_node_t;

typedef struct tmpfs_ctx_t {
  kmutex_t      lock;
  tmpfs_node_t* head;
} tmpfs_ctx_t;

typedef struct tmpfs_file_state_t {
  tmpfs_ctx_t*  ctx;
  tmpfs_node_t* node;
  size_t        offset;
  int           flags;
} tmpfs_file_state_t;

static bool tmpfs_reserve(tmpfs_node_t* node, size_t new_capacity);

static tmpfs_node_t* tmpfs_find_node(tmpfs_ctx_t* ctx, const char* name) {
  tmpfs_node_t* node = ctx->head;
  while(node != NULL) {
    if(strcmp(node->name, name) == 0) {
      return node;
    }
    node = node->next;
  }
  return NULL;
}

static void tmpfs_detach_node(tmpfs_ctx_t* ctx, tmpfs_node_t* target) {
  tmpfs_node_t* prev = NULL;
  tmpfs_node_t* node = ctx->head;
  while(node != NULL) {
    if(node == target) {
      break;
    }
    prev = node;
    node = node->next;
  }
  if(node == NULL) {
    return;
  }
  if(prev != NULL) {
    prev->next = node->next;
  } else {
    ctx->head = node->next;
  }
  if(node->data != NULL) {
    kfree(node->data);
  }
  kfree(node);
}

static tmpfs_node_t* tmpfs_create_node(tmpfs_ctx_t* ctx, const char* name) {
  tmpfs_node_t* node = kmalloc(sizeof(tmpfs_node_t));
  if(node == NULL) {
    return NULL;
  }
  memset(node, 0, sizeof(*node));
  strncpy(node->name, name, sizeof(node->name) - 1);
  node->name[sizeof(node->name) - 1] = '\0';
  node->data = NULL;
  node->size = 0;
  node->capacity = 0;
  node->deleted = false;
  node->refcount = 0;
  node->next = ctx->head;
  ctx->head = node;
  return node;
}

static bool tmpfs_list(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context) {
  tmpfs_ctx_t* ctx = (tmpfs_ctx_t*)fs_ctx;
  if(path != NULL && path[0] != '\0' && strcmp(path, "/") != 0) {
    return false;
  }

  kmutex_lock(&ctx->lock);
  for(tmpfs_node_t* node = ctx->head; node != NULL; node = node->next) {
    if(node->deleted) {
      continue;
    }
    vfs_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, node->name, sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.is_directory = false;
    entry.size = (uint32_t)node->size;
    if(!iter(&entry, context)) {
      break;
    }
  }
  kmutex_unlock(&ctx->lock);
  return true;
}

static bool tmpfs_read(void* fs_ctx,
                       const char* path,
                       size_t offset,
                       void* buffer,
                       size_t length,
                       size_t* bytes_read) {
  tmpfs_ctx_t* ctx = (tmpfs_ctx_t*)fs_ctx;
  if(buffer == NULL || bytes_read == NULL) {
    return false;
  }

  const char* name = path;
  while(*name == '/') {
    name++;
  }

  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = tmpfs_find_node(ctx, name);
  if(node == NULL || node->deleted) {
    kmutex_unlock(&ctx->lock);
    return false;
  }

  if(offset >= node->size) {
    *bytes_read = 0;
    kmutex_unlock(&ctx->lock);
    return true;
  }

  size_t available = node->size - offset;
  size_t to_copy = (length < available) ? length : available;
  memcpy(buffer, node->data + offset, to_copy);
  *bytes_read = to_copy;
  kmutex_unlock(&ctx->lock);
  return true;
}

static bool tmpfs_read_all(void* fs_ctx, const char* path, void** out_buffer, size_t* out_size) {
  tmpfs_ctx_t* ctx = (tmpfs_ctx_t*)fs_ctx;
  if(out_buffer == NULL || out_size == NULL) {
    return false;
  }

  const char* name = path;
  while(*name == '/') {
    name++;
  }

  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = tmpfs_find_node(ctx, name);
  if(node == NULL || node->deleted) {
    kmutex_unlock(&ctx->lock);
    return false;
  }

  void* copy = kmalloc(node->size);
  if(copy == NULL) {
    kmutex_unlock(&ctx->lock);
    return false;
  }
  memcpy(copy, node->data, node->size);
  *out_buffer = copy;
  *out_size = node->size;
  kmutex_unlock(&ctx->lock);
  return true;
}

static bool tmpfs_driver_write(void* fs_ctx,
                                const char* path,
                                size_t offset,
                                const void* buffer,
                                size_t length,
                                size_t* bytes_written) {
  tmpfs_ctx_t* ctx = (tmpfs_ctx_t*)fs_ctx;
  if(buffer == NULL) {
    return false;
  }

  const char* name = path;
  while(*name == '/') {
    name++;
  }

  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = tmpfs_find_node(ctx, name);
  if(node == NULL || node->deleted) {
    kmutex_unlock(&ctx->lock);
    return false;
  }

  if(offset > node->size) {
    kmutex_unlock(&ctx->lock);
    return false;
  }

  size_t end = offset + length;
  if(!tmpfs_reserve(node, end)) {
    kmutex_unlock(&ctx->lock);
    return false;
  }

  memcpy(node->data + offset, buffer, length);
  if(end > node->size) {
    node->size = end;
  }

  if(bytes_written) {
    *bytes_written = length;
  }

  kmutex_unlock(&ctx->lock);
  return true;
}

static bool tmpfs_driver_write_all(void* fs_ctx, const char* path, const void* buffer, size_t size) {
  tmpfs_ctx_t* ctx = (tmpfs_ctx_t*)fs_ctx;
  if(buffer == NULL) {
    return false;
  }

  const char* name = path;
  while(*name == '/') {
    name++;
  }

  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = tmpfs_find_node(ctx, name);
  if(node == NULL || node->deleted) {
    kmutex_unlock(&ctx->lock);
    return false;
  }

  if(!tmpfs_reserve(node, size)) {
    kmutex_unlock(&ctx->lock);
    return false;
  }

  memcpy(node->data, buffer, size);
  node->size = size;
  kmutex_unlock(&ctx->lock);
  return true;
}

static int tmpfs_close(file_t* file) {
  tmpfs_file_state_t* state = (tmpfs_file_state_t*)file->private_data;
  if(state == NULL) {
    return 0;
  }

  tmpfs_ctx_t* ctx = state->ctx;
  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = state->node;
  if(node != NULL) {
    if(node->refcount > 0) {
      node->refcount--;
    }
    if(node->refcount == 0 && node->deleted) {
      tmpfs_detach_node(ctx, node);
    }
  }
  kmutex_unlock(&ctx->lock);

  kfree(state);
  file->private_data = NULL;
  return 0;
}

static int64_t tmpfs_read_impl(file_t* file, void* buffer, size_t length) {
  if(file == NULL || buffer == NULL) {
    return -EINVAL;
  }

  tmpfs_file_state_t* state = (tmpfs_file_state_t*)file->private_data;
  if(state == NULL || state->node == NULL) {
    return -EIO;
  }

  tmpfs_ctx_t* ctx = state->ctx;
  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = state->node;
  if(node->deleted) {
    kmutex_unlock(&ctx->lock);
    return -ENOENT;
  }

  if(state->offset >= node->size) {
    kmutex_unlock(&ctx->lock);
    return 0;
  }

  size_t available = node->size - state->offset;
  size_t to_copy = (length < available) ? length : available;
  memcpy(buffer, node->data + state->offset, to_copy);
  state->offset += to_copy;
  kmutex_unlock(&ctx->lock);
  return (int64_t)to_copy;
}

static bool tmpfs_reserve(tmpfs_node_t* node, size_t new_capacity) {
  if(new_capacity <= node->capacity) {
    return true;
  }

  size_t alloc = (node->capacity == 0) ? 256 : node->capacity;
  while(alloc < new_capacity) {
    alloc *= 2;
  }

  uint8_t* resized = krealloc(node->data, alloc);
  if(resized == NULL) {
    return false;
  }
  node->data = resized;
  node->capacity = alloc;
  return true;
}

static int64_t tmpfs_write_impl(file_t* file, const void* buffer, size_t length) {
  if(file == NULL || buffer == NULL) {
    return -EINVAL;
  }

  tmpfs_file_state_t* state = (tmpfs_file_state_t*)file->private_data;
  if(state == NULL || state->node == NULL) {
    return -EIO;
  }

  tmpfs_ctx_t* ctx = state->ctx;
  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = state->node;
  if(node->deleted) {
    kmutex_unlock(&ctx->lock);
    return -ENOENT;
  }

  if((state->flags & O_APPEND) != 0) {
    state->offset = node->size;
  }

  size_t end = state->offset + length;
  if(!tmpfs_reserve(node, end)) {
    kmutex_unlock(&ctx->lock);
    return -ENOMEM;
  }

  memcpy(node->data + state->offset, buffer, length);
  state->offset = end;
  if(end > node->size) {
    node->size = end;
  }
  kmutex_unlock(&ctx->lock);
  return (int64_t)length;
}

static int64_t tmpfs_seek_impl(file_t* file, int64_t offset, int whence) {
  tmpfs_file_state_t* state = (tmpfs_file_state_t*)file->private_data;
  if(state == NULL || state->node == NULL) {
    return -EINVAL;
  }

  tmpfs_ctx_t* ctx = state->ctx;
  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = state->node;
  size_t base = 0;
  switch(whence) {
    case SEEK_SET:
      base = 0;
      break;
    case SEEK_CUR:
      base = state->offset;
      break;
    case SEEK_END:
      base = node->size;
      break;
    default:
      kmutex_unlock(&ctx->lock);
      return -EINVAL;
  }

  int64_t new_pos = (int64_t)base + offset;
  if(new_pos < 0) {
    kmutex_unlock(&ctx->lock);
    return -EINVAL;
  }
  state->offset = (size_t)new_pos;
  kmutex_unlock(&ctx->lock);
  return (int64_t)state->offset;
}

static const file_ops_t tmpfs_file_ops = {
  .read = tmpfs_read_impl,
  .write = tmpfs_write_impl,
  .close = tmpfs_close,
  .seek = tmpfs_seek_impl,
  .ioctl = NULL,
};

static uint32_t tmpfs_requested_mode(int flags) {
  switch(flags & 0x3) {
    case O_WRONLY:
      return FILE_MODE_WRITE;
    case O_RDWR:
      return FILE_MODE_READ | FILE_MODE_WRITE;
    default:
      return FILE_MODE_READ;
  }
}

static int tmpfs_open(void* fs_ctx, const char* path, int flags, file_t** out_file) {
  tmpfs_ctx_t* ctx = (tmpfs_ctx_t*)fs_ctx;
  if(path == NULL || out_file == NULL) {
    return -EINVAL;
  }

  const char* name = path;
  while(*name == '/') {
    name++;
  }
  if(name[0] == '\0') {
    serial_printf("tmpfs_open: reject path '%s' (empty)\n", path);
    return -ENOSYS;
  }
  for(const char* it = name; *it != '\0'; ++it) {
    if(*it == '/') {
      serial_printf("tmpfs_open: reject path '%s' (nested segment)\n", path);
      return -ENOSYS;
    }
  }

  bool create = (flags & O_CREAT) != 0;
  bool exclusive = (flags & O_EXCL) != 0;
  bool trunc = (flags & O_TRUNC) != 0;

  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = tmpfs_find_node(ctx, name);
  if(node == NULL) {
    if(!create) {
      serial_printf("tmpfs_open: '%s' not found\n", name);
      kmutex_unlock(&ctx->lock);
      return -ENOENT;
    }
    node = tmpfs_create_node(ctx, name);
    if(node == NULL) {
      serial_printf("tmpfs_open: '%s' allocation failed\n", name);
      kmutex_unlock(&ctx->lock);
      return -ENOMEM;
    }
    serial_printf("tmpfs_open: created '%s'\n", name);
  } else {
    if(create && exclusive) {
      serial_printf("tmpfs_open: '%s' exists with O_EXCL\n", name);
      kmutex_unlock(&ctx->lock);
      return -EEXIST;
    }
    if(trunc) {
      if(node->data != NULL) {
        kfree(node->data);
        node->data = NULL;
      }
      node->size = 0;
      node->capacity = 0;
    }
    node->deleted = false;
  }

  node->refcount++;
  kmutex_unlock(&ctx->lock);

  tmpfs_file_state_t* state = kmalloc(sizeof(tmpfs_file_state_t));
  if(state == NULL) {
    kmutex_lock(&ctx->lock);
    node->refcount--;
    kmutex_unlock(&ctx->lock);
    return -ENOMEM;
  }

  state->ctx = ctx;
  state->node = node;
  state->flags = flags;
  state->offset = ((flags & O_APPEND) != 0) ? node->size : 0;

  uint32_t mode = tmpfs_requested_mode(flags);
  file_t* file = file_create(&tmpfs_file_ops, state, mode);
  if(file == NULL) {
    kmutex_lock(&ctx->lock);
    node->refcount--;
    if(node->refcount == 0 && node->deleted) {
      tmpfs_detach_node(ctx, node);
    }
    kmutex_unlock(&ctx->lock);
    kfree(state);
    serial_printf("tmpfs_open: file_create failed for '%s'\n", name);
    return -ENOMEM;
  }

  serial_printf("tmpfs_open: opened '%s' flags=0x%x\n", name, flags);
  *out_file = file;
  return 0;
}

static int tmpfs_unlink(void* fs_ctx, const char* path) {
  tmpfs_ctx_t* ctx = (tmpfs_ctx_t*)fs_ctx;
  if(path == NULL) {
    return -EINVAL;
  }

  const char* name = path;
  while(*name == '/') {
    name++;
  }

  kmutex_lock(&ctx->lock);
  tmpfs_node_t* node = tmpfs_find_node(ctx, name);
  if(node == NULL) {
    kmutex_unlock(&ctx->lock);
    return -ENOENT;
  }

  if(node->refcount > 0) {
    node->deleted = true;
    kmutex_unlock(&ctx->lock);
    return 0;
  }

  tmpfs_detach_node(ctx, node);
  kmutex_unlock(&ctx->lock);
  return 0;
}

static void tmpfs_destroy(void* fs_ctx) {
  tmpfs_ctx_t* ctx = (tmpfs_ctx_t*)fs_ctx;
  if(ctx == NULL) {
    return;
  }
  tmpfs_node_t* node = ctx->head;
  while(node != NULL) {
    tmpfs_node_t* next = node->next;
    if(node->data != NULL) {
      kfree(node->data);
    }
    kfree(node);
    node = next;
  }
  kfree(ctx);
}

static const vfs_fs_driver_t tmpfs_driver = {
  .list = tmpfs_list,
  .read = tmpfs_read,
  .read_all = tmpfs_read_all,
  .write = tmpfs_driver_write,
  .write_all = tmpfs_driver_write_all,
  .open = tmpfs_open,
  .unlink = tmpfs_unlink,
  .create_file = NULL,
  .truncate_file = NULL,
  .stat = NULL,
  .destroy = tmpfs_destroy,
};

bool tmpfs_mount(void) {
  tmpfs_ctx_t* ctx = kmalloc(sizeof(tmpfs_ctx_t));
  if(ctx == NULL) {
    return false;
  }
  kmutex_init(&ctx->lock);
  ctx->head = NULL;
  if(!vfs_mount("/tmp", &tmpfs_driver, ctx, false)) {
    kfree(ctx);
    return false;
  }
  return true;
}
