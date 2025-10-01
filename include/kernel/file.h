#ifndef MENIOS_INCLUDE_KERNEL_FILE_H
#define MENIOS_INCLUDE_KERNEL_FILE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <types.h>

struct proc_info_t;

typedef struct file file_t;

typedef struct file_ops_t {
  int64_t (*read)(file_t* file, void* buffer, size_t length);
  int64_t (*write)(file_t* file, const void* buffer, size_t length);
  int (*close)(file_t* file);
  int64_t (*seek)(file_t* file, int64_t offset, int whence);
} file_ops_t;

struct file {
  const file_ops_t* ops;
  void*             private_data;
  int64_t           refcount;
  uint32_t          mode;
};

#define FILE_MODE_READ   (1u << 0)
#define FILE_MODE_WRITE  (1u << 1)

#define PROC_MAX_FILES 256

typedef struct file_descriptor_entry_t {
  file_t*  file;
  uint32_t flags;
} file_descriptor_entry_t;

#define FD_FLAG_CLOEXEC  (1u << 0)

void file_system_init(void);
file_t* file_create(const file_ops_t* ops, void* private_data, uint32_t mode);
void file_ref(file_t* file);
void file_unref(file_t* file);
int64_t file_read(file_t* file, void* buffer, size_t length);
int64_t file_write(file_t* file, const void* buffer, size_t length);
int64_t file_seek(file_t* file, int64_t offset, int whence);

void proc_file_table_init(struct proc_info_t* proc);
void proc_file_table_clone(struct proc_info_t* child, struct proc_info_t* parent);
void proc_file_table_cleanup(struct proc_info_t* proc);
void proc_file_table_prepare_exec(struct proc_info_t* proc);
int  proc_file_install(struct proc_info_t* proc, file_t* file, uint32_t flags);
int  proc_file_install_at(struct proc_info_t* proc, int fd, file_t* file, uint32_t flags);
file_t* proc_file_get(struct proc_info_t* proc, int fd, uint32_t* flags_out);
int  proc_file_set_flags(struct proc_info_t* proc, int fd, uint32_t flags);
int  proc_file_close(struct proc_info_t* proc, int fd);
int  proc_file_dup(struct proc_info_t* proc, int oldfd, int newfd, bool cloexec);

file_t* file_create_serial_console_file(void);
file_t* file_create_framebuffer_console_file(void);
file_t* file_create_tty_console_file(void);

typedef file_t* file_descriptor_t;
file_descriptor_t fd_get(int fd);
int pipe_create(file_t** read_end, file_t** write_end);
void stdin_enqueue_char(uint8_t ch);

#endif
