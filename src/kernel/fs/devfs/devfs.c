#include <kernel/fs/devfs/devfs.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <kernel/file.h>
#include <kernel/fs/core.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>
#include <kernel/tsc.h>

#define CHAR_DEVICE_DYNAMIC_MAJOR_MIN 10u
#define CHAR_DEVICE_MAJOR_COUNT      (MENIOS_DEV_MAJOR_MASK + 1u)

typedef struct devfs_entry_t {
  char_device_t*        device;
  struct devfs_entry_t* next;
} devfs_entry_t;

typedef struct char_minor_allocation_t {
  unsigned int                    start;
  unsigned int                    count;
  char_device_t*                  device;
  struct char_minor_allocation_t* next;
} char_minor_allocation_t;

typedef struct {
  bool                   reserved;
  char_minor_allocation_t* allocations;
} char_major_state_t;

static kmutex_t       devfs_lock;
static devfs_entry_t* devfs_devices;
static bool           devfs_ready;
static unsigned int   next_dynamic_major = CHAR_DEVICE_DYNAMIC_MAJOR_MIN;
static char_major_state_t char_major_table[CHAR_DEVICE_MAJOR_COUNT];

static void devfs_init_once(void) {
  if(devfs_ready) {
    return;
  }
  kmutex_init(&devfs_lock);
  devfs_devices = NULL;
  next_dynamic_major = CHAR_DEVICE_DYNAMIC_MAJOR_MIN;
  for(unsigned int major = 0; major < CHAR_DEVICE_DYNAMIC_MAJOR_MIN && major < CHAR_DEVICE_MAJOR_COUNT; ++major) {
    char_major_table[major].reserved = true;
  }
  devfs_ready = true;
}

static devfs_entry_t* devfs_find_by_name(const char* name) {
  for(devfs_entry_t* entry = devfs_devices; entry != NULL; entry = entry->next) {
    if(strcmp(entry->device->name, name) == 0) {
      return entry;
    }
  }
  return NULL;
}

static devfs_entry_t* devfs_find_by_dev(dev_t dev) {
  unsigned int target_major = MAJOR(dev);
  unsigned int target_minor = MINOR(dev);
  for(devfs_entry_t* entry = devfs_devices; entry != NULL; entry = entry->next) {
    const char_device_t* device = entry->device;
    unsigned int device_major = MAJOR(device->dev);
    if(device_major != target_major) {
      continue;
    }
    unsigned int device_minor = MINOR(device->dev);
    unsigned int range = device->minor_count == 0 ? 1u : device->minor_count;
    if(target_minor >= device_minor && target_minor < device_minor + range) {
      return entry;
    }
  }
  return NULL;
}

static bool char_major_has_overlap(unsigned int major, unsigned int start_minor, unsigned int count) {
  if(major >= CHAR_DEVICE_MAJOR_COUNT) {
    return true;
  }
  char_minor_allocation_t* allocation = char_major_table[major].allocations;
  unsigned int end = start_minor + count - 1u;
  while(allocation != NULL) {
    unsigned int allocation_start = allocation->start;
    unsigned int allocation_end = allocation_start + allocation->count - 1u;
    if(!(end < allocation_start || start_minor > allocation_end)) {
      return true;
    }
    allocation = allocation->next;
  }
  return false;
}

static int char_major_track_range(char_device_t* device,
                                  unsigned int major,
                                  unsigned int start_minor,
                                  unsigned int count) {
  if(major >= CHAR_DEVICE_MAJOR_COUNT) {
    return -ERANGE;
  }
  char_major_state_t* state = &char_major_table[major];
  if(char_major_has_overlap(major, start_minor, count)) {
    return -EEXIST;
  }
  char_minor_allocation_t* allocation = kmalloc(sizeof(char_minor_allocation_t));
  if(allocation == NULL) {
    return -ENOMEM;
  }
  allocation->start = start_minor;
  allocation->count = count;
  allocation->device = device;
  allocation->next = state->allocations;
  state->allocations = allocation;
  return 0;
}

static void char_major_untrack_range(char_device_t* device) {
  if(device == NULL) {
    return;
  }
  unsigned int major = MAJOR(device->dev);
  if(major >= CHAR_DEVICE_MAJOR_COUNT) {
    return;
  }
  char_major_state_t* state = &char_major_table[major];
  char_minor_allocation_t** cursor = &state->allocations;
  while(*cursor != NULL) {
    if((*cursor)->device == device) {
      char_minor_allocation_t* victim = *cursor;
      *cursor = victim->next;
      kfree(victim);
      return;
    }
    cursor = &(*cursor)->next;
  }
}

static bool char_major_is_reserved(unsigned int major) {
  if(major >= CHAR_DEVICE_MAJOR_COUNT) {
    return true;
  }
  return char_major_table[major].reserved;
}

static bool char_major_is_unused(unsigned int major) {
  if(major >= CHAR_DEVICE_MAJOR_COUNT) {
    return false;
  }
  if(char_major_table[major].reserved) {
    return false;
  }
  return char_major_table[major].allocations == NULL;
}

static dev_t allocate_dynamic_dev(unsigned int minor_count) {
  unsigned int count = minor_count == 0 ? 1u : minor_count;
  if(count == 0 || count > (MENIOS_DEV_MINOR_MASK + 1u)) {
    return 0;
  }
  unsigned int start_major = next_dynamic_major;
  if(start_major < CHAR_DEVICE_DYNAMIC_MAJOR_MIN) {
    start_major = CHAR_DEVICE_DYNAMIC_MAJOR_MIN;
  }

  unsigned int major = start_major;
  for(int pass = 0; pass < 2; ++pass) {
    while(major < CHAR_DEVICE_MAJOR_COUNT) {
      if(char_major_is_unused(major)) {
        next_dynamic_major = major + 1;
        return MKDEV(major, 0);
      }
      major++;
    }
    major = CHAR_DEVICE_DYNAMIC_MAJOR_MIN;
  }
  return 0;
}

static mode_t devfs_mode_for_device(const char_device_t* device) {
  mode_t perms = 0;
  if(device->access_mode & FILE_MODE_READ) {
    perms |= 0444;
  }
  if(device->access_mode & FILE_MODE_WRITE) {
    perms |= 0222;
  }
  if(perms == 0) {
    perms = 0000;
  }
  return S_IFCHR | perms;
}

static void devfs_fill_info(const char_device_t* device, fs_path_info_t* out_info) {
  memset(out_info, 0, sizeof(*out_info));
  out_info->block_size = 4096;
  out_info->inode = (uint64_t)device->dev;
  out_info->is_directory = false;
  out_info->size = 0;
  out_info->has_mode = true;
  out_info->mode = devfs_mode_for_device(device);
  out_info->is_read_only = (device->access_mode & FILE_MODE_WRITE) == 0;
  out_info->has_rdev = true;
  out_info->rdev = device->dev;
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

static bool devfs_list(void* fs_ctx, const char* path, vfs_dir_iter_t iter, void* context) {
  (void)fs_ctx;
  if(path != NULL && path[0] != '\0' && strcmp(path, "/") != 0) {
    return false;
  }

  kmutex_lock(&devfs_lock);
  for(devfs_entry_t* entry = devfs_devices; entry != NULL; entry = entry->next) {
    vfs_dir_entry_t dir_entry;
    memset(&dir_entry, 0, sizeof(dir_entry));
    strncpy(dir_entry.name, entry->device->name, sizeof(dir_entry.name) - 1);
    dir_entry.name[sizeof(dir_entry.name) - 1] = '\0';
    dir_entry.size = 0;
    dir_entry.is_directory = false;
    if(!iter(&dir_entry, context)) {
      break;
    }
  }
  kmutex_unlock(&devfs_lock);
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

static bool devfs_lookup_device(const char* path, char_device_t** out_device) {
  while(*path == '/') {
    path++;
  }
  kmutex_lock(&devfs_lock);
  devfs_entry_t* entry = devfs_find_by_name(path);
  if(entry == NULL) {
    kmutex_unlock(&devfs_lock);
    return false;
  }
  *out_device = entry->device;
  kmutex_unlock(&devfs_lock);
  return true;
}

static int devfs_open(void* fs_ctx, const char* path, int flags, file_t** out_file) {
  (void)fs_ctx;
  if(path == NULL || out_file == NULL) {
    return -EINVAL;
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

  char_device_t* device = NULL;
  if(!devfs_lookup_device(path, &device)) {
    return -ENOENT;
  }

  if((device->access_mode & requested) != requested) {
    return -EACCES;
  }

  file_t* file = NULL;
  int rc = device->open(device, flags, &file);
  if(rc != 0) {
    return rc;
  }
  if(file == NULL) {
    return -ENOMEM;
  }

  file->mode = device->access_mode;
  file->private_data = device;
  *out_file = file;
  return 0;
}

static int devfs_unlink(void* fs_ctx, const char* path) {
  (void)fs_ctx;
  (void)path;
  return -ENOSYS;
}

static void devfs_destroy(void* fs_ctx) {
  (void)fs_ctx;
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

  char_device_t* device = NULL;
  if(!devfs_lookup_device(path, &device)) {
    return false;
  }

  devfs_fill_info(device, out_info);
  return true;
}

static int devfs_file_stat(file_t* file, struct stat* out_stat) {
  if(file == NULL || out_stat == NULL) {
    return -EINVAL;
  }

  char_device_t* device = (char_device_t*)file->private_data;
  if(device == NULL) {
    return -EINVAL;
  }

  fs_path_info_t info;
  devfs_fill_info(device, &info);
  fs_path_info_to_stat(&info, out_stat);
  out_stat->st_rdev = device->dev;
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
  .mknod = NULL,
  .chmod = NULL,
  .utimens = NULL,
  .destroy = devfs_destroy,
};

static int devfs_register_entry(char_device_t* device) {
  devfs_entry_t* entry = kmalloc(sizeof(devfs_entry_t));
  if(entry == NULL) {
    return -ENOMEM;
  }
  entry->device = device;
  entry->next = devfs_devices;
  devfs_devices = entry;
  return 0;
}

static void devfs_unregister_entry(char_device_t* device) {
  devfs_entry_t** cursor = &devfs_devices;
  while(*cursor) {
    if((*cursor)->device == device) {
      devfs_entry_t* victim = *cursor;
      *cursor = victim->next;
      kfree(victim);
      return;
    }
    cursor = &(*cursor)->next;
  }
}

int char_device_register(char_device_t* device) {
  if(device == NULL || device->name == NULL || device->open == NULL) {
    return -EINVAL;
  }
  if(device->access_mode == 0) {
    return -EINVAL;
  }

  devfs_init_once();

  kmutex_lock(&devfs_lock);

  if(devfs_find_by_name(device->name) != NULL) {
    kmutex_unlock(&devfs_lock);
    return -EEXIST;
  }

  unsigned int requested_count = device->minor_count == 0 ? 1u : device->minor_count;
  if(requested_count > (MENIOS_DEV_MINOR_MASK + 1u)) {
    kmutex_unlock(&devfs_lock);
    return -ERANGE;
  }

  bool allocated_dynamic = false;
  dev_t original_dev = device->dev;

  if(device->dev == 0) {
    dev_t dev = allocate_dynamic_dev(requested_count);
    if(dev == 0) {
      kmutex_unlock(&devfs_lock);
      return -ENOSPC;
    }
    device->dev = dev;
    allocated_dynamic = true;
  }

  unsigned int major = MAJOR(device->dev);
  unsigned int base_minor = MINOR(device->dev);
  if(base_minor + requested_count - 1u > MENIOS_DEV_MINOR_MASK) {
    device->dev = allocated_dynamic ? original_dev : device->dev;
    kmutex_unlock(&devfs_lock);
    return -ERANGE;
  }

  if(char_major_has_overlap(major, base_minor, requested_count)) {
    if(allocated_dynamic) {
      device->dev = original_dev;
    }
    kmutex_unlock(&devfs_lock);
    return -EEXIST;
  }

  int range_rc = char_major_track_range(device, major, base_minor, requested_count);
  if(range_rc != 0) {
    if(allocated_dynamic) {
      device->dev = original_dev;
    }
    kmutex_unlock(&devfs_lock);
    return range_rc;
  }

  int rc = devfs_register_entry(device);
  if(rc == 0) {
    device->minor_count = requested_count;
    unsigned int major = MAJOR(device->dev);
    if(major >= next_dynamic_major) {
      next_dynamic_major = major + 1;
    }
  } else {
    char_major_untrack_range(device);
    if(allocated_dynamic) {
      device->dev = original_dev;
    }
  }
  kmutex_unlock(&devfs_lock);
  return rc;
}

void char_device_unregister(char_device_t* device) {
  if(device == NULL) {
    return;
  }

  devfs_init_once();
  kmutex_lock(&devfs_lock);
  devfs_unregister_entry(device);
  char_major_untrack_range(device);
  kmutex_unlock(&devfs_lock);
}

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

static int devfs_null_open(char_device_t* device, int flags, file_t** out_file) {
  (void)flags;
  file_t* file = file_create(&devfs_null_ops, device, device->access_mode);
  if(file == NULL) {
    return -ENOMEM;
  }
  file->private_data = device;
  *out_file = file;
  return 0;
}

static int devfs_zero_open(char_device_t* device, int flags, file_t** out_file) {
  (void)flags;
  file_t* file = file_create(&devfs_zero_ops, device, device->access_mode);
  if(file == NULL) {
    return -ENOMEM;
  }
  file->private_data = device;
  *out_file = file;
  return 0;
}

static int devfs_tty0_open(char_device_t* device, int flags, file_t** out_file) {
  (void)flags;
  file_t* file = file_create_tty_console_file();
  if(file == NULL) {
    return -ENOMEM;
  }
  file->private_data = device;
  *out_file = file;
  return 0;
}

static int devfs_console_open(char_device_t* device, int flags, file_t** out_file) {
  (void)flags;
  file_t* file = file_create_framebuffer_console_file();
  if(file == NULL) {
    return -ENOMEM;
  }
  file->private_data = device;
  *out_file = file;
  return 0;
}

static int devfs_fb0_open(char_device_t* device, int flags, file_t** out_file) {
  (void)flags;
  file_t* file = file_create_framebuffer_device_file();
  if(file == NULL) {
    return -ENOMEM;
  }
  file->private_data = device;
  *out_file = file;
  return 0;
}

static int devfs_serial_open(char_device_t* device, int flags, file_t** out_file) {
  (void)flags;
  file_t* file = file_create_serial_console_file();
  if(file == NULL) {
    return -ENOMEM;
  }
  file->private_data = device;
  *out_file = file;
  return 0;
}

static char_device_t null_device = {
  .name = "null",
  .access_mode = FILE_MODE_WRITE,
  .open = devfs_null_open,
  .driver_data = NULL,
  .dev = MKDEV(1, 3),
  .minor_count = 1,
};

static char_device_t zero_device = {
  .name = "zero",
  .access_mode = FILE_MODE_READ,
  .open = devfs_zero_open,
  .driver_data = NULL,
  .dev = MKDEV(1, 5),
  .minor_count = 1,
};

static char_device_t tty0_device = {
  .name = "tty0",
  .access_mode = FILE_MODE_READ | FILE_MODE_WRITE,
  .open = devfs_tty0_open,
  .driver_data = NULL,
  .dev = MKDEV(4, 0),
  .minor_count = 1,
};

static char_device_t fb0_device = {
  .name = "fb0",
  .access_mode = FILE_MODE_READ | FILE_MODE_WRITE,
  .open = devfs_fb0_open,
  .driver_data = NULL,
  .dev = MKDEV(29, 0),
  .minor_count = 1,
};

static char_device_t console_device = {
  .name = "console",
  .access_mode = FILE_MODE_WRITE,
  .open = devfs_console_open,
  .driver_data = NULL,
  .dev = MKDEV(5, 1),
  .minor_count = 1,
};

static char_device_t ttyS0_device = {
  .name = "ttyS0",
  .access_mode = FILE_MODE_WRITE,
  .open = devfs_serial_open,
  .driver_data = NULL,
  .dev = MKDEV(4, 64),
  .minor_count = 1,
};

void char_device_system_init(void) {
  devfs_init_once();

  static bool builtins_registered = false;
  if(builtins_registered) {
    return;
  }
  builtins_registered = true;

  if(char_device_register(&null_device) != 0) {
    serial_printf("char_device_system_init: failed to register /dev/null\n");
  }
  if(char_device_register(&zero_device) != 0) {
    serial_printf("char_device_system_init: failed to register /dev/zero\n");
  }
  if(char_device_register(&tty0_device) != 0) {
    serial_printf("char_device_system_init: failed to register /dev/tty0\n");
  }
  if(char_device_register(&fb0_device) != 0) {
    serial_printf("char_device_system_init: failed to register /dev/fb0\n");
  }
  if(char_device_register(&console_device) != 0) {
    serial_printf("char_device_system_init: failed to register /dev/console\n");
  }
  if(char_device_register(&ttyS0_device) != 0) {
    serial_printf("char_device_system_init: failed to register /dev/ttyS0\n");
  }
}

void char_device_iterate(char_device_iter_fn fn, void* context) {
  if(fn == NULL) {
    return;
  }
  devfs_init_once();
  kmutex_lock(&devfs_lock);
  for(devfs_entry_t* entry = devfs_devices; entry != NULL; entry = entry->next) {
    fn(entry->device, context);
  }
  kmutex_unlock(&devfs_lock);
}

char_device_t* char_device_lookup(dev_t dev) {
  devfs_init_once();
  kmutex_lock(&devfs_lock);
  devfs_entry_t* entry = devfs_find_by_dev(dev);
  char_device_t* device = entry ? entry->device : NULL;
  kmutex_unlock(&devfs_lock);
  return device;
}

void char_device_reserve_major(unsigned int major) {
  devfs_init_once();
  if(major >= CHAR_DEVICE_MAJOR_COUNT) {
    return;
  }
  kmutex_lock(&devfs_lock);
  char_major_table[major].reserved = true;
  kmutex_unlock(&devfs_lock);
}

bool devfs_mount(void) {
  char_device_system_init();
  return vfs_mount("/dev", &devfs_driver, NULL, true);
}
