#include <kernel/condvar.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/input.h>
#include <kernel/mman.h>
#include <kernel/pmm.h>
#include <kernel/mutex.h>
#include <kernel/proc.h>
#include <kernel/shm.h>
#include <kernel/syscall.h>
#include <kernel/vm.h>
#include <kernel/thread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

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
proc_info_p procs[PROC_MAX] = { &kernel_process_info };

int test_stub_acpi_shutdown_calls = 0;
int test_stub_acpi_shutdown_result = 0;

int acpi_shutdown(void) {
  test_stub_acpi_shutdown_calls++;
  return test_stub_acpi_shutdown_result;
}

int chdir(const char* path) {
  if(path == NULL) {
    errno = EFAULT;
    return -1;
  }
  return 0;
}

static void stub_init_proc(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }
  proc->cwd[0] = '/';
  proc->cwd[1] = '\0';
  proc->cwd_len = 1;
}

static void __attribute__((constructor)) stub_kernel_proc_init(void) {
  stub_init_proc(&kernel_process_info);
  current = &kernel_process_info;
}

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

void proc_request_block(void) {}

void proc_mark_ready(proc_info_p proc) {
  if(proc) {
    proc->state = PROC_STATE_READY;
  }
}

void proc_mark_stopped(proc_info_p proc, int signo) {
  if(proc == NULL) {
    return;
  }

  proc->stop_status = ((signo & 0x7f) << 8) | 0x7f;
  proc->stop_status_pending = true;
  proc->stopped = true;
  proc->state = PROC_STATE_STOPPED;
  proc->time_slice_remaining_us = 0;
  proc->continue_status = 0;
  proc->continued_pending = false;
}

void proc_mark_continued(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  proc->stopped = false;
  proc->stop_status_pending = false;
  proc->continue_status = 0xffff;
  proc->continued_pending = true;
  if(proc->state == PROC_STATE_STOPPED) {
    proc->state = PROC_STATE_READY;
  }
}

cpu_state_p proc_switch(cpu_state_p state) {
  return state;
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

bool proc_user_copy_in(proc_info_p proc, void* dest, virt_addr_t src, size_t length) {
  (void)proc;
  if(length == 0) {
    return true;
  }
  memcpy(dest, (const void*)(uintptr_t)src, length);
  return true;
}

bool proc_user_copy_out(proc_info_p proc, virt_addr_t dest, const void* src, size_t length) {
  (void)proc;
  if(length == 0) {
    return true;
  }
  memcpy((void*)(uintptr_t)dest, src, length);
  return true;
}

bool proc_user_write64(proc_info_p proc, virt_addr_t addr, uint64_t value) {
  (void)proc;
  uint64_t* slot = (uint64_t*)(uintptr_t)addr;
  *slot = value;
  return true;
}

__attribute__((weak)) bool fs_file_create(const fs_mount_t* mount, const char* path, bool exclusive) {
  (void)mount;
  (void)path;
  (void)exclusive;
  return false;
}

__attribute__((weak)) bool fs_file_truncate(const fs_mount_t* mount, const char* path) {
  (void)mount;
  (void)path;
  return false;
}

__attribute__((weak)) bool fs_path_info(const fs_mount_t* mount, const char* path, fs_path_info_t* out_info) {
  (void)mount;
  (void)path;
  if(out_info != NULL) {
    memset(out_info, 0, sizeof(*out_info));
  }
  return false;
}

__attribute__((weak)) int fat32_chmod_impl(void* fs_ctx, const char* path, mode_t mode) {
  (void)fs_ctx;
  (void)path;
  (void)mode;
  errno = ENOSYS;
  return -1;
}

__attribute__((weak)) int fat32_utimens_path(void* fs_ctx, const char* path, const struct timespec times[2]) {
  (void)fs_ctx;
  (void)path;
  (void)times;
  errno = ENOSYS;
  return -1;
}

__attribute__((weak)) bool fs_file_stat(const fs_mount_t* mount, const char* path, size_t* out_size) {
  (void)mount;
  (void)path;
  if(out_size) {
    *out_size = 0;
  }
  return false;
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

bool proc_shm_track_attachment(proc_info_p proc,
                               shm_region_t* region,
                               virt_addr_t base,
                               size_t length,
                               int shmid,
                               int flags) {
  if(proc == NULL || region == NULL || length == 0) {
    return false;
  }

  if(proc->shm_attachment_count >= PROC_MAX_SHM_ATTACHMENTS) {
    return false;
  }

  proc_shm_attachment_t* slot = &proc->shm_attachments[proc->shm_attachment_count++];
  slot->region = region;
  slot->base = base;
  slot->length = length;
  slot->shmid = shmid;
  slot->flags = flags;
  shm_region_increment_attachments(region);
  return true;
}

bool proc_shm_remove_attachment(proc_info_p proc,
                                virt_addr_t base,
                                proc_shm_attachment_t* out) {
  if(proc == NULL) {
    return false;
  }

  for(size_t i = 0; i < proc->shm_attachment_count; ++i) {
    if(proc->shm_attachments[i].base == base) {
      if(out != NULL) {
        *out = proc->shm_attachments[i];
      }
      for(size_t j = i + 1; j < proc->shm_attachment_count; ++j) {
        proc->shm_attachments[j - 1] = proc->shm_attachments[j];
      }
      proc->shm_attachment_count--;
      return true;
    }
  }
  return false;
}

bool vm_map_physical(proc_info_p proc,
                     virt_addr_t base,
                     phys_addr_t phys,
                     size_t length,
                     uint32_t flags) {
  (void)proc;
  (void)base;
  (void)phys;
  (void)length;
  (void)flags;
  return true;
}

bool proc_user_touch_range(proc_info_p proc,
                           virt_addr_t addr,
                           size_t length,
                           bool write) {
  (void)proc;
  (void)addr;
  (void)length;
  (void)write;
  return true;
}

bool keyboard_event_try_pop(menios_key_event_t* event) {
  (void)event;
  return false;
}

void keyboard_event_push(const menios_key_event_t* event) {
  (void)event;
}

void proc_shm_detach_all(proc_info_p proc) {
  if(proc == NULL) {
    return;
  }

  while(proc->shm_attachment_count > 0) {
    proc_shm_attachment_t attachment =
      proc->shm_attachments[proc->shm_attachment_count - 1];
    if(!proc_shm_detach(proc, attachment.base)) {
      break;
    }
  }
}

bool proc_shm_inherit(proc_info_p child, proc_info_p parent) {
  if(child == NULL || parent == NULL) {
    return false;
  }

  size_t original = child->shm_attachment_count;

  for(size_t i = 0; i < parent->shm_attachment_count; ++i) {
    proc_shm_attachment_t* attachment = &parent->shm_attachments[i];
    uint32_t flags = VM_REGION_FLAG_USER | VM_REGION_FLAG_READ;
    bool writable = (attachment->flags & SHM_RDONLY) == 0;
    if(writable) {
      flags |= VM_REGION_FLAG_WRITE;
    }

    shm_region_t* region = attachment->region;
    shm_region_ref(region);

    if(!vm_map_shared(child, region, attachment->base, flags, writable)) {
      shm_region_unref(region);
      goto inherit_fail;
    }

    if(!proc_shm_track_attachment(child,
                                  region,
                                  attachment->base,
                                  attachment->length,
                                  attachment->shmid,
                                  attachment->flags)) {
      vm_unmap(child, attachment->base, attachment->length);
      shm_region_unref(region);
      goto inherit_fail;
    }

    virt_addr_t end = attachment->base + attachment->length;
    virt_addr_t next = (end + PAGE_SIZE - 1) & ~((virt_addr_t)PAGE_SIZE - 1);
    if(next > child->mmap_next) {
      child->mmap_next = next;
    }
  }

  return true;

inherit_fail:
  while(child->shm_attachment_count > original) {
    proc_shm_attachment_t rollback = child->shm_attachments[child->shm_attachment_count - 1];
    vm_unmap(child, rollback.base, rollback.length);
    child->shm_attachment_count--;
    shm_region_decrement_attachments(rollback.region);
    shm_region_unref(rollback.region);
  }
  return false;
}

bool proc_shm_detach(proc_info_p proc, virt_addr_t base) {
  if(proc == NULL) {
    return false;
  }

  size_t index = 0;
  bool found = false;
  proc_shm_attachment_t attachment;

  for(; index < proc->shm_attachment_count; ++index) {
    if(proc->shm_attachments[index].base == base) {
      attachment = proc->shm_attachments[index];
      found = true;
      break;
    }
  }

  if(!found) {
    return false;
  }

  if(!vm_unmap(proc, attachment.base, attachment.length)) {
    return false;
  }

  for(size_t j = index + 1; j < proc->shm_attachment_count; ++j) {
    proc->shm_attachments[j - 1] = proc->shm_attachments[j];
  }
  proc->shm_attachment_count--;

  shm_region_decrement_attachments(attachment.region);
  shm_region_unref(attachment.region);
  return true;
}

static bool vm_stub_add_region(proc_info_p proc,
                               virt_addr_t base,
                               size_t length,
                               vm_region_type_t type,
                               uint32_t flags) {
  if(proc == NULL || length == 0) {
    return false;
  }

  if(vm_range_overlaps(proc, base, length)) {
    return false;
  }

  if(!vm_region_add(proc, base, length, type, flags)) {
    return false;
  }

  vm_region_t* region = vm_region_find(proc, base);
  if(region != NULL) {
    region->committed_base = base;
    region->committed_top = base + length;
  }
  return true;
}

bool vm_range_overlaps(proc_info_p proc, virt_addr_t base, size_t length) {
  if(proc == NULL || length == 0) {
    return false;
  }

  virt_addr_t end = base + length;
  for(size_t i = 0; i < proc->vm_region_count; ++i) {
    vm_region_t* region = &proc->vm_regions[i];
    virt_addr_t region_end = region->base + region->length;
    if(!(end <= region->base || base >= region_end)) {
      return true;
    }
  }
  return false;
}

bool vm_map(proc_info_p proc, const vm_map_params_t* params) {
  if(proc == NULL || params == NULL || params->length == 0) {
    return false;
  }
  return vm_stub_add_region(proc, params->base, params->length, params->type, params->flags);
}

bool vm_unmap(proc_info_p proc, virt_addr_t base, size_t length) {
  if(proc == NULL || length == 0) {
    return false;
  }

  for(size_t i = 0; i < proc->vm_region_count; ++i) {
    vm_region_t* region = &proc->vm_regions[i];
    if(region->base == base && region->length == length) {
      for(size_t j = i + 1; j < proc->vm_region_count; ++j) {
        proc->vm_regions[j - 1] = proc->vm_regions[j];
      }
      proc->vm_region_count--;
      return true;
    }
  }
  return false;
}

bool vm_map_shared(proc_info_p proc,
                   shm_region_t* region,
                   virt_addr_t base,
                   uint32_t flags,
                   bool writable) {
  (void)region;
  (void)writable;
  size_t length = shm_region_page_count(region) * PAGE_SIZE;
  return vm_stub_add_region(proc, base, length, VM_REGION_SHARED, flags);
}

bool vm_clone(proc_info_p dst, proc_info_p src) {
  if(dst == NULL || src == NULL) {
    return false;
  }

  for(size_t i = 0; i < src->vm_region_count; ++i) {
    vm_region_t* region = &src->vm_regions[i];
    if(!vm_stub_add_region(dst, region->base, region->length, region->type, region->flags)) {
      return false;
    }
  }
  return true;
}

bool proc_register_user_segment(proc_info_p proc, phys_addr_t phys, size_t pages) {
  if(proc == NULL || pages == 0) {
    return false;
  }

  if(proc->user_segment_count >= PROC_MAX_USER_SEGMENTS) {
    return false;
  }

  proc_user_segment_t* seg = &proc->user_segments[proc->user_segment_count++];
  seg->phys = phys;
  seg->pages = pages;
  return true;
}

void proc_unregister_user_segment(proc_info_p proc, phys_addr_t phys, size_t pages) {
  if(proc == NULL || proc->user_segment_count == 0) {
    return;
  }

  for(size_t i = 0; i < proc->user_segment_count; ++i) {
    proc_user_segment_t* seg = &proc->user_segments[i];
    if(seg->phys == phys && seg->pages == pages) {
      for(size_t j = i + 1; j < proc->user_segment_count; ++j) {
        proc->user_segments[j - 1] = proc->user_segments[j];
      }
      proc->user_segment_count--;
      return;
    }
  }
}

bool block_device_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count) {
  if(device == NULL || buffer == NULL || block_count == 0) {
    return false;
  }
  if(device->ops == NULL || device->ops->read_blocks == NULL) {
    return false;
  }
  return device->ops->read_blocks(device, lba, buffer, block_count);
}

bool block_device_write(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count) {
  if(device == NULL || buffer == NULL || block_count == 0) {
    return false;
  }
  if(device->ops == NULL || device->ops->write_blocks == NULL) {
    return false;
  }
  return device->ops->write_blocks(device, lba, buffer, block_count);
}

bool block_device_flush(block_device_t* device) {
  if(device == NULL) {
    return false;
  }
  if(device->ops == NULL || device->ops->flush == NULL) {
    return true;
  }
  return device->ops->flush(device);
}

bool __attribute__((weak)) fs_mount_fat32_first(block_device_t* device, fs_mount_t** out_mount) {
  (void)device;
  (void)out_mount;
  return false;
}

bool __attribute__((weak)) fs_mount_fat32_partition(block_device_t* device,
                                                    uint32_t partition_index,
                                                    fs_mount_t** out_mount) {
  (void)device;
  (void)partition_index;
  (void)out_mount;
  return false;
}

void __attribute__((weak)) fs_unmount(fs_mount_t* mount) {
  (void)mount;
}

bool __attribute__((weak)) fs_list_directory(const fs_mount_t* mount,
                                            const char* path,
                                            fs_dir_iter_t iter,
                                            void* context) {
  (void)mount;
  (void)path;
  (void)iter;
  (void)context;
  return false;
}

bool __attribute__((weak)) fs_file_read(const fs_mount_t* mount,
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

bool __attribute__((weak)) fs_file_read_all(const fs_mount_t* mount,
                                            const char* path,
                                            void** out_buffer,
                                            size_t* out_size) {
  (void)mount;
  (void)path;
  (void)out_buffer;
  (void)out_size;
  return false;
}

bool __attribute__((weak)) fs_directory_create(const fs_mount_t* mount,
                                               const char* path,
                                               bool exclusive) {
  (void)mount;
  (void)path;
  (void)exclusive;
  return false;
}

bool __attribute__((weak)) fs_path_unlink(const fs_mount_t* mount, const char* path) {
  (void)mount;
  (void)path;
  return false;
}

bool __attribute__((weak)) fs_directory_remove(const fs_mount_t* mount, const char* path) {
  (void)mount;
  (void)path;
  return false;
}

bool __attribute__((weak)) fs_file_write(const fs_mount_t* mount,
                                         const char* path,
                                         size_t offset,
                                         const void* buffer,
                                         size_t length,
                                         size_t* bytes_written) {
  (void)mount;
  (void)path;
  (void)offset;
  (void)buffer;
  (void)length;
  (void)bytes_written;
  return false;
}

bool __attribute__((weak)) fs_file_write_all(const fs_mount_t* mount,
                                            const char* path,
                                            const void* buffer,
                                            size_t size) {
  (void)mount;
  (void)path;
  (void)buffer;
  (void)size;
  return false;
}

int __attribute__((weak)) fat32_open_adapter(void* fs_ctx, const char* path, int flags, file_t** out_file) {
  (void)fs_ctx;
  (void)path;
  (void)flags;
  (void)out_file;
  return -ENOSYS;
}

int __attribute__((weak)) fat32_unlink_adapter(void* fs_ctx, const char* path) {
  (void)fs_ctx;
  (void)path;
  return -ENOSYS;
}

void* __attribute__((weak)) kmalloc(size_t size) {
  return malloc(size);
}

void __attribute__((weak)) kfree(void* ptr) {
  free(ptr);
}

void* __attribute__((weak)) krealloc(void* ptr, size_t size) {
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

void* __attribute__((weak)) kcalloc(size_t nelem, size_t elsize) {
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
    current->err_no = ENOSYS;
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

int proc_waitpid(proc_info_p parent, int pid, int options, int* status_out) {
  if(parent == NULL) {
    return -ECHILD;
  }

  proc_info_p prev = NULL;
  proc_info_p child = parent->first_child;
  bool report_stopped = (options & WUNTRACED) != 0;
  bool report_continued = (options & WCONTINUED) != 0;

  while(child != NULL) {
    proc_info_p next = child->sibling_next;
    if(pid > 0 && (int)child->pid != pid) {
      prev = child;
      child = next;
      continue;
    }

    if(child->stop_status_pending && report_stopped) {
      if(status_out != NULL) {
        *status_out = child->stop_status;
      }
      child->stop_status_pending = false;
      parent->waitpid_waiting = false;
      parent->waitpid_target = -1;
      return (int)child->pid;
    }

    if(child->continued_pending && report_continued) {
      if(status_out != NULL) {
        *status_out = child->continue_status;
      }
      child->continued_pending = false;
      parent->waitpid_waiting = false;
      parent->waitpid_target = -1;
      return (int)child->pid;
    }

    if(child->state == PROC_STATE_ZOMBIE) {
      if(status_out != NULL) {
        *status_out = child->exit_code;
      }
      if(prev != NULL) {
        prev->sibling_next = next;
      } else {
        parent->first_child = next;
      }
      if(parent->children_count > 0) {
        parent->children_count--;
      }
      child->state = PROC_STATE_TERMINATED;
      child->sibling_next = NULL;
      child->parent = NULL;
      parent->waitpid_waiting = false;
      parent->waitpid_target = -1;
      return (int)child->pid;
    }

    parent->waitpid_waiting = true;
    parent->waitpid_target = (pid > 0) ? pid : -1;
    return 0;
  }

  return -ECHILD;
}

void proc_exit(int status) {
  if(current != NULL) {
    current->exit_code = status;
    current->state = PROC_STATE_ZOMBIE;
  }
}

void proc_exit_signal(int signo) {
  proc_exit(signo & 0x7f);
}

bool proc_user_buffer_accessible(proc_info_p proc, const void* ptr, size_t length) {
  (void)proc;
  (void)ptr;
  (void)length;
  return true;
}

int proc_kill_pid(uint32_t pid, int code) {
  (void)pid;
  (void)code;
  return -ENOSYS;
}

proc_info_p proc_find_by_pid(uint32_t pid) {
  for(size_t index = 0; index < PROC_MAX; index++) {
    proc_info_p proc = procs[index];
    if(proc != NULL && proc->pid == pid) {
      return proc;
    }
  }
  return NULL;
}
