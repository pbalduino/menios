#include <kernel/devfs.h>

#include <errno.h>
#include <string.h>
#include <sys/fcntl.h>

#include <kernel/file.h>
#include <kernel/serial.h>
#include <kernel/vfs.h>

typedef struct devfs_node_t {
  const char*        name;
  uint32_t           mode;
  file_t*           (*factory)(void);
} devfs_node_t;

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
};

static const file_ops_t devfs_zero_ops = {
  .read = devfs_zero_read,
  .write = devfs_null_write,
  .close = NULL,
  .seek = NULL,
  .ioctl = NULL,
};

static file_t* devfs_create_null(void) {
  return file_create(&devfs_null_ops, NULL, FILE_MODE_WRITE);
}

static file_t* devfs_create_zero(void) {
  return file_create(&devfs_zero_ops, NULL, FILE_MODE_READ);
}

static file_t* devfs_create_tty0(void) {
  return file_create_tty_console_file();
}

static file_t* devfs_create_console(void) {
  return file_create_framebuffer_console_file();
}

static file_t* devfs_create_ttys0(void) {
  return file_create_serial_console_file();
}

static const devfs_node_t devfs_nodes[] = {
  { "null",     FILE_MODE_WRITE,              devfs_create_null     },
  { "zero",     FILE_MODE_READ,               devfs_create_zero     },
  { "tty0",     FILE_MODE_READ | FILE_MODE_WRITE, devfs_create_tty0     },
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
    if((node->mode & requested) != requested) {
      return -EACCES;
    }
    file_t* file = node->factory();
    if(file == NULL) {
      return -ENOMEM;
    }
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

static const vfs_fs_driver_t devfs_driver = {
  .list = devfs_list,
  .read = devfs_read,
  .read_all = devfs_read_all,
  .write = NULL,
  .write_all = NULL,
  .create_file = NULL,
  .truncate_file = NULL,
  .stat = NULL,
  .open = devfs_open,
  .unlink = devfs_unlink,
  .destroy = devfs_destroy,
};

bool devfs_mount(void) {
  return vfs_mount("/dev", &devfs_driver, NULL, true);
}
