#include <kernel/condvar.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/mman.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#ifdef serial_printf
#undef serial_printf
#endif
#ifdef serial_puts
#undef serial_puts
#endif
#ifdef serial_error
#undef serial_error
#endif
#ifdef serial_line
#undef serial_line
#endif

proc_info_p current;
proc_info_t kernel_process_info;

void disable_interrupts() {}

void enable_interrupts() {}

void halt() {
  exit(1);
}

uint64_t boot_time(void) {
  return 0;
}

void proc_yield(void) {}

void (*test_proc_request_yield_hook)(void) = NULL;

void proc_request_yield(void) {
  if(test_proc_request_yield_hook) {
    test_proc_request_yield_hook();
  }
}

void proc_request_sleep(uint64_t duration_us) {
  (void)duration_us;
}

void proc_mark_ready(proc_info_p proc) {
  if(proc) {
    proc->state = PROC_STATE_READY;
  }
}

void proc_switch(void* frame) {
  (void)frame;
}

void memzero(void* s, uint64_t n) {
	memset(s, 0, n);
}

void* memsetl(void* v, int64_t c, size_t n) {
  if(n == 0) {
    return v;
  }

  if(((uintptr_t)v % 8) == 0 && (n % 8) == 0) {
    uint64_t* dest = (uint64_t*)v;
    for(size_t i = 0; i < n; i++) {
      dest[i] = (uint64_t)c;
    }
    return v;
  }

  memset(v, (int)(uint8_t)c, n);
  return v;
}

static void vdiscard(const char* fmt, va_list args) {
  (void)fmt;
  (void)args;
}

void serial_printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vdiscard(fmt, args);
  va_end(args);
}

void serial_puts(const char* str) {
  (void)str;
}

void serial_error(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vdiscard(fmt, args);
  va_end(args);
}

void serial_line(const char* str) {
  (void)str;
}

void logk(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vdiscard(fmt, args);
  va_end(args);
}

void serial_putchar(char ch) {
  (void)ch;
}

void fb_putchar(char ch) {
  (void)ch;
}

bool fs_mount_fat32_first(block_device_t* device, fs_mount_t** out_mount) {
  (void)device;
  (void)out_mount;
  return false;
}

bool fs_mount_fat32_partition(block_device_t* device, uint32_t partition_index, fs_mount_t** out_mount) {
  (void)device;
  (void)partition_index;
  (void)out_mount;
  return false;
}

void fs_unmount(fs_mount_t* mount) {
  (void)mount;
}

bool fs_list_directory(const fs_mount_t* mount, const char* path, fs_dir_iter_t iter, void* context) {
  (void)mount;
  (void)path;
  (void)iter;
  (void)context;
  return false;
}

bool fs_file_read(const fs_mount_t* mount,
                  const char* path,
                  size_t offset,
                  void* buffer,
                  size_t length,
                  size_t* bytes_read) {
  (void)mount;
  (void)path;
  (void)offset;
  (void)buffer;
  (void)length;
  (void)bytes_read;
  return false;
}

bool fs_file_read_all(const fs_mount_t* mount,
                      const char* path,
                      void** out_buffer,
                      size_t* out_size) {
  (void)mount;
  (void)path;
  (void)out_buffer;
  (void)out_size;
  return false;
}

int pipe_create(file_t** read_end, file_t** write_end) {
  if(read_end) {
    *read_end = NULL;
  }
  if(write_end) {
    *write_end = NULL;
  }
  return -ENOSYS;
}

void* kmalloc(size_t size) {
  return malloc(size);
}

void kfree(void* ptr) {
  free(ptr);
}

void* krealloc(void* ptr, size_t size) {
  if(size == 0) {
    free(ptr);
    return NULL;
  }
  void* new_ptr = malloc(size);
  if(new_ptr == NULL) {
    return NULL;
  }
  if(ptr) {
    free(ptr);
  }
  return new_ptr;
}

void* kcalloc(size_t nelem, size_t elsize) {
  size_t total = nelem * elsize;
  void* ptr = malloc(total);
  if(ptr) {
    memset(ptr, 0, total);
  }
  return ptr;
}

void* kmmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  (void)addr;
  (void)length;
  (void)prot;
  (void)flags;
  (void)fd;
  (void)offset;
  if(current) {
    current->errno = ENOSYS;
  }
  return MAP_FAILED;
}

int kmunmap(void* addr, size_t len) {
  (void)addr;
  (void)len;
  return -ENOSYS;
}

proc_info_p proc_fork(proc_info_p parent, const syscall_frame_t* frame, int* err) {
  (void)parent;
  (void)frame;
  if(err) {
    *err = -ENOSYS;
  }
  return NULL;
}

int proc_exec_image(proc_info_p proc,
                    const uint8_t* image,
                    size_t size,
                    syscall_frame_t* frame,
                    const proc_exec_args_t* args) {
  (void)proc;
  (void)image;
  (void)size;
  (void)frame;
  (void)args;
  return -ENOSYS;
}

int proc_waitpid(proc_info_p parent, int pid, int* status_out) {
  (void)parent;
  (void)pid;
  (void)status_out;
  return -ENOSYS;
}

void proc_exit(int status) {
  (void)status;
}

bool proc_user_buffer_accessible(proc_info_p proc, const void* ptr, size_t length) {
  (void)proc;
  (void)ptr;
  (void)length;
  return true;
}
