#include <kernel/fs/devfs/devfs.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <kernel/file.h>
#include <kernel/fs/core.h>
#include <kernel/tsc.h>
#include <kernel/serial.h>
#include <kernel/fs/vfs/vfs.h>

typedef struct devfs_node_t {
  const char*        name;
  uint32_t           access_mode;
  file_t*           (*factory)(const struct devfs_node_t* node);
} devfs_node_t;

static void devfs_fill_info(const devfs_node_t* node, fs_path_info_t* out_info);
static int devfs_file_stat(file_t* file, struct stat* out_stat);

static int64_t devfs_null_read(file_t* file, void* buffer, size_t length) {
  (void)file;
  (void)buffer;
  (void)length;
  return 0;
}

static int64_t devfs_null_write(file_t* file, const void* buffer, size_t length) {
  (void)file;
  (void)buffer;
  return (int64_t)length;
}

static int64_t devfs_zero_read(file_t* file, void* buffer, size_t length) {
  if(buffer == NULL) {
    return -EINVAL;
  }
  memset(buffer, 0, length);
  return (int64_t)length;
}

static const file_ops_t devfs_null_ops = {
  .read = devfs_null_read,
  .write = devfs_null_write,
  .close = NULL,
  .seek = NULL,
  .ioctl = NULL,
  .mmap = NULL,
  .stat = devfs_file_stat,
  .chmod = NULL,
  .utimens = NULL,
};

static const file_ops_t devfs_zero_ops = {
  .read = devfs_zero_read,
  .write = devfs_null_write,
  .close = NULL,
  .seek = NULL,
  .ioctl = NULL,
  .mmap = NULL,
  .stat = devfs_file_stat,
  .chmod = NULL,
  .utimens = NULL,
};

static file_t* devfs_create_null(const devfs_node_t* node) {
  return file_create(&devfs_null_ops, (void*)node, node->access_mode);
}

static file_t* devfs_create_zero(const devfs_node_t* node) {
  return file_create(&devfs_zero_ops, (void*)node, node->access_mode);
}

static file_t* devfs_create_tty0(const devfs_node_t* node) {
  file_t* file = file_create_tty_console_file();
  if(file != NULL) {
    file->private_data = (void*)node;
  }
  return file;
}

static file_t* devfs_create_console(const devfs_node_t* node) {
  file_t* file = file_create_framebuffer_console_file();
  if(file != NULL) {
    file->private_data = (void*)node;
  }
  return file;
}

static file_t* devfs_create_fb0(const devfs_node_t* node) {
  file_t* file = file_create_framebuffer_device_file();
  if(file != NULL) {
    file->private_data = (void*)node;
  }
  return file;
}

static file_t* devfs_create_ttys0(const devfs_node_t* node) {
  file_t* file = file_create_serial_console_file();
  if(file != NULL) {
    file->private_data = (void*)node;
  }
  return file;
}

static const devfs_node_t devfs_nodes[] = {
  { "null",     FILE_MODE_WRITE,              devfs_create_null     },
  { "zero",     FILE_MODE_READ,               devfs_create_zero     },
  { "tty0",     FILE_MODE_READ | FILE_MODE_WRITE, devfs_create_tty0     },
  { "fb0",      FILE_MODE_READ | FILE_MODE_WRITE, devfs_create_fb0     },
  { "console",  FILE_MODE_WRITE,              devfs_create_console  },
  { "ttyS0",    FILE_MODE_WRITE,              devfs_create_ttys0    },
};

static size_t devfs_node_count(void) {
  return sizeof(devfs_nodes) / sizeof(devfs_nodes[0]);
}

static bool devfs_list(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context) {
  (void)fs_ctx;
  if(path != NULL && path[0] != '\0' && strcmp(path, "/") != 0) {
    return false;
  }

  for(size_t i = 0; i < devfs_node_count(); i++) {
    vfs_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, devfs_nodes[i].name, sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.size = 0;
    entry.is_directory = false;
    if(!iter(&entry, context)) {
      break;
    }
  }
  return true;
}

static bool devfs_read(void* fs_ctx,
                       const char* path,
                       size_t offset,
                       void* buffer,
                       size_t length,
                       size_t* bytes_read) {
  (void)fs_ctx;
  (void)path;
  (void)offset;
  (void)buffer;
  (void)length;
  (void)bytes_read;
  return false;
}

static bool devfs_read_all(void* fs_ctx, const char* path, void** out_buffer, size_t* out_size) {
  (void)fs_ctx;
  (void)path;
  (void)out_buffer;
  (void)out_size;
  return false;
}

static int devfs_open(void* fs_ctx, const char* path, int flags, file_t** out_file) {
  (void)fs_ctx;
  if(path == NULL || out_file == NULL) {
    return -EINVAL;
  }

  while(*path == '/') {
    path++;
  }

  uint32_t requested = 0;
  switch(flags & 0x3) {
    case O_RDONLY:
      requested = FILE_MODE_READ;
      break;
    case O_WRONLY:
      requested = FILE_MODE_WRITE;
      break;
    case O_RDWR:
      requested = FILE_MODE_READ | FILE_MODE_WRITE;
      break;
    default:
      requested = FILE_MODE_READ;
      break;
  }

  for(size_t i = 0; i < devfs_node_count(); i++) {
    const devfs_node_t* node = &devfs_nodes[i];
    if(strcmp(path, node->name) != 0) {
      continue;
    }
    if((node->access_mode & requested) != requested) {
      return -EACCES;
    }
    file_t* file = node->factory(node);
    if(file == NULL) {
      return -ENOMEM;
    }
    file->mode = node->access_mode;
    *out_file = file;
    return 0;
  }

  return -ENOENT;
}

static int devfs_unlink(void* fs_ctx, const char* path) {
  (void)fs_ctx;
  (void)path;
  return -ENOSYS;
}

static void devfs_destroy(void* fs_ctx) {
  (void)fs_ctx;
}

static mode_t devfs_mode_for_node(const devfs_node_t* node) {
  mode_t perms = 0;
  if(node->access_mode & FILE_MODE_READ) {
    perms |= 0444;
  }
  if(node->access_mode & FILE_MODE_WRITE) {
    perms |= 0222;
  }
  return S_IFCHR | perms;
}

static void devfs_fill_info(const devfs_node_t* node, fs_path_info_t* out_info) {
  memset(out_info, 0, sizeof(*out_info));
  out_info->block_size = 4096;
  out_info->inode = node ? (uint64_t)(node - devfs_nodes + 1) : 0;
  out_info->is_directory = false;
  out_info->size = 0;
  out_info->has_mode = true;
  out_info->mode = devfs_mode_for_node(node);
  out_info->is_read_only = (node->access_mode & FILE_MODE_WRITE) == 0;
  out_info->has_times = true;
  uint64_t usec = unix_time_us();
  struct timespec now = {
    .tv_sec = (time_t)(usec / 1000000ull),
    .tv_nsec = (long)((usec % 1000000ull) * 1000ull),
  };
  out_info->atime = now;
  out_info->mtime = now;
  out_info->ctime = now;
}

static bool devfs_stat(void* fs_ctx, const char* path, fs_path_info_t* out_info) {
  (void)fs_ctx;
  if(path == NULL || out_info == NULL) {
    return false;
  }

  if(path[0] == '\0' || strcmp(path, "/") == 0) {
    memset(out_info, 0, sizeof(*out_info));
    out_info->is_directory = true;
    out_info->has_mode = true;
    out_info->mode = S_IFDIR | 0555;
    out_info->is_read_only = true;
    out_info->block_size = 4096;
    out_info->has_times = true;
    uint64_t usec = unix_time_us();
    struct timespec now = {
      .tv_sec = (time_t)(usec / 1000000ull),
      .tv_nsec = (long)((usec % 1000000ull) * 1000ull),
    };
    out_info->atime = now;
    out_info->mtime = now;
    out_info->ctime = now;
    return true;
  }

  while(*path == '/') {
    path++;
  }

  for(size_t i = 0; i < devfs_node_count(); i++) {
    const devfs_node_t* node = &devfs_nodes[i];
    if(strcmp(path, node->name) == 0) {
      devfs_fill_info(node, out_info);
      return true;
    }
  }
  return false;
}

static int devfs_file_stat(file_t* file, struct stat* out_stat) {
  if(file == NULL || out_stat == NULL) {
    return -EINVAL;
  }
  const devfs_node_t* node = (const devfs_node_t*)file->private_data;
  if(node == NULL) {
    return -EINVAL;
  }
  fs_path_info_t info;
  devfs_fill_info(node, &info);
  fs_path_info_to_stat(&info, out_stat);
  out_stat->st_rdev = (dev_t)(node - devfs_nodes + 1);
  return 0;
}

static const vfs_fs_driver_t devfs_driver = {
  .list = devfs_list,
  .read = devfs_read,
  .read_all = devfs_read_all,
  .write = NULL,
  .write_all = NULL,
  .create_file = NULL,
  .truncate_file = NULL,
  .stat = devfs_stat,
  .open = devfs_open,
  .unlink = devfs_unlink,
  .mkdir = NULL,
  .rmdir = NULL,
  .rename = NULL,
  .chmod = NULL,
  .utimens = NULL,
  .destroy = devfs_destroy,
};

bool devfs_mount(void) {
  return vfs_mount("/dev", &devfs_driver, NULL, true);
}
