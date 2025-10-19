#include <errno.h>
#include <kernel/console.h>
#include <kernel/file.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/mman.h>
#include <kernel/proc.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <kernel/shm.h>
#include <kernel/signal.h>
#include <kernel/syscall.h>
#include <kernel/syscall_entry.h>
#include <kernel/tsc.h>
#include <kernel/vfs.h>
#include <kernel/vm.h>
#include <kernel/input.h>
#include <menios/signal_frame.h>
#include <menios/input.h>
#include <sys/fcntl.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <stdint.h>

#ifndef MENIOS_HOST_TEST

#ifndef CONFIG_DEBUG_SYSCALL
#define CONFIG_DEBUG_SYSCALL 0
#endif

#if CONFIG_DEBUG_SYSCALL
#define SYSCALL_TRACE_ENABLED 1
#else
#define SYSCALL_TRACE_ENABLED 0
#endif

extern uint64_t syscall_last_return_value;
extern uint64_t syscall_last_return_slot_value;

#if SYSCALL_TRACE_ENABLED
void syscall_trace_return(uint64_t value) {
  serial_printf("[sysret-trace] pid=%u rax=%lx\n",
                current ? current->pid : 0u,
                (unsigned long)value);
}
#else
void syscall_trace_return(uint64_t value) {
  (void)value;
}
#endif

#if SYSCALL_TRACE_ENABLED
#define SYSCALL_TRACE(...) serial_printf(__VA_ARGS__)
#else
#define SYSCALL_TRACE(...) ((void)0)
#endif

#else

#define SYSCALL_TRACE_ENABLED 0
#define SYSCALL_TRACE(...) ((void)0)

#endif

#define SYSCALL_MAX 256

static uint64_t syscall_stub_unimplemented(syscall_frame_t* frame);
static uint64_t syscall_read_handler(syscall_frame_t* frame);
static uint64_t syscall_write_handler(syscall_frame_t* frame);
static uint64_t syscall_close_handler(syscall_frame_t* frame);
static uint64_t syscall_open_handler(syscall_frame_t* frame);
static uint64_t syscall_lseek_handler(syscall_frame_t* frame);
static uint64_t syscall_mmap_handler(syscall_frame_t* frame);
static uint64_t syscall_munmap_handler(syscall_frame_t* frame);
static uint64_t syscall_pipe_handler(syscall_frame_t* frame);
static uint64_t syscall_dup_handler(syscall_frame_t* frame);
static uint64_t syscall_dup2_handler(syscall_frame_t* frame);
static uint64_t syscall_fork_handler(syscall_frame_t* frame);
static uint64_t syscall_execve_handler(syscall_frame_t* frame);
static uint64_t syscall_yield_handler(syscall_frame_t* frame);
static uint64_t syscall_sleep_handler(syscall_frame_t* frame);
static uint64_t syscall_nanosleep_handler(syscall_frame_t* frame);
static uint64_t syscall_clock_gettime_handler(syscall_frame_t* frame);
static uint64_t syscall_clock_settime_handler(syscall_frame_t* frame);
static uint64_t syscall_clock_getres_handler(syscall_frame_t* frame);
static uint64_t syscall_setitimer_handler(syscall_frame_t* frame);
static uint64_t syscall_getitimer_handler(syscall_frame_t* frame);
static uint64_t syscall_alarm_handler(syscall_frame_t* frame);
static uint64_t syscall_sigreturn_handler(syscall_frame_t* frame);
static uint64_t syscall_exit_handler(syscall_frame_t* frame);
static uint64_t syscall_fcntl_handler(syscall_frame_t* frame);
static uint64_t syscall_waitpid_handler(syscall_frame_t* frame);
static uint64_t syscall_listdir_handler(syscall_frame_t* frame);
static uint64_t syscall_stdin_poll_handler(syscall_frame_t* frame);
static uint64_t syscall_input_event_handler(syscall_frame_t* frame);
static uint64_t syscall_chdir_handler(syscall_frame_t* frame);
static uint64_t syscall_getcwd_handler(syscall_frame_t* frame);
static uint64_t syscall_unlink_handler(syscall_frame_t* frame);
static uint64_t syscall_mkdir_handler(syscall_frame_t* frame);
static uint64_t syscall_rmdir_handler(syscall_frame_t* frame);
static uint64_t syscall_rename_handler(syscall_frame_t* frame);
static uint64_t syscall_proc_kill_handler(syscall_frame_t* frame);
static uint64_t syscall_kill_handler(syscall_frame_t* frame);
static uint64_t syscall_sigaction_handler(syscall_frame_t* frame);
static uint64_t syscall_sigprocmask_handler(syscall_frame_t* frame);
static uint64_t syscall_getsockopt_handler(syscall_frame_t* frame);
static uint64_t syscall_proc_list_handler(syscall_frame_t* frame);
static uint64_t syscall_ioctl_handler(syscall_frame_t* frame);
static uint64_t syscall_shmget_handler(syscall_frame_t* frame);
static uint64_t syscall_shmat_handler(syscall_frame_t* frame);
static uint64_t syscall_shmdt_handler(syscall_frame_t* frame);
static uint64_t syscall_shmctl_handler(syscall_frame_t* frame);
static uint64_t syscall_getpagesize_handler(syscall_frame_t* frame);
static uint64_t syscall_time_handler(syscall_frame_t* frame);
static uint64_t syscall_gettimeofday_handler(syscall_frame_t* frame);

static syscall_handler_t syscall_table[SYSCALL_MAX];

#define SYSCALL_PATH_MAX 256
#define EXECVE_MAX_ARGS   64
#define EXECVE_MAX_ENVP   64
#define EXECVE_MAX_STRING 4096

static int64_t realtime_offset_us = 0;

static size_t page_align_up_size(size_t value) {
  if(value == 0) {
    return PAGE_SIZE;
  }
  size_t remainder = value % PAGE_SIZE;
  if(remainder == 0) {
    return value;
  }
  return value + (PAGE_SIZE - remainder);
}

static virt_addr_t page_align_down_addr(virt_addr_t value) {
  return value & ~((virt_addr_t)PAGE_SIZE - 1);
}

static virt_addr_t page_align_up_addr(virt_addr_t value) {
  if((value & (PAGE_SIZE - 1)) == 0) {
    return value;
  }
  return (value + PAGE_SIZE) & ~((virt_addr_t)PAGE_SIZE - 1);
}

static bool check_overflow(virt_addr_t base, size_t length) {
  return length > (size_t)(UINT64_MAX - base);
}

static uint32_t prot_to_region_flags(int prot) {
  uint32_t flags = VM_REGION_FLAG_USER;
  if(prot & PROT_READ) {
    flags |= VM_REGION_FLAG_READ;
  }
  if(prot & PROT_WRITE) {
    flags |= VM_REGION_FLAG_WRITE;
  }
  if(prot & PROT_EXEC) {
    flags |= VM_REGION_FLAG_EXEC;
  }
  return flags;
}

static bool mmap_select_base(void* addr_hint,
                             size_t aligned_len,
                             virt_addr_t* base_out,
                             bool* used_hint) {
  if(current == NULL) {
    return false;
  }

  virt_addr_t base_hint = current->mmap_next ? current->mmap_next : current->mmap_base;
  virt_addr_t base = addr_hint ? page_align_down_addr((virt_addr_t)addr_hint)
                               : page_align_up_addr(base_hint);
  bool hint = (addr_hint != NULL);

  if(check_overflow(base, aligned_len)) {
    return false;
  }

  virt_addr_t end = base + aligned_len;
  if(base < current->mmap_base || end > current->mmap_limit || base >= end) {
    return false;
  }

  if(!hint) {
    while(end <= current->mmap_limit && vm_range_overlaps(current, base, aligned_len)) {
      base = page_align_up_addr(end);
      if(check_overflow(base, aligned_len)) {
        return false;
      }
      end = base + aligned_len;
    }

    if(end > current->mmap_limit || base >= end) {
      return false;
    }
  } else if(vm_range_overlaps(current, base, aligned_len)) {
    return false;
  }

  if(base_out != NULL) {
    *base_out = base;
  }
  if(used_hint != NULL) {
    *used_hint = hint;
  }
  return true;
}

static uint64_t realtime_now_us(void) {
  uint64_t base = unix_time_us();
  int64_t adjusted = (int64_t)base + realtime_offset_us;
  if(adjusted < 0) {
    adjusted = 0;
  }
  return (uint64_t)adjusted;
}

static void fill_timespec_from_us(struct timespec* ts, uint64_t usec) {
  if(ts == NULL) {
    return;
  }
  ts->tv_sec = (time_t)(usec / 1000000ull);
  uint64_t rem = usec % 1000000ull;
  ts->tv_nsec = (long)(rem * 1000ull);
}

static void fill_timespec_from_ns(struct timespec* ts, uint64_t nsec) {
  if(ts == NULL) {
    return;
  }
  ts->tv_sec = (time_t)(nsec / 1000000000ull);
  ts->tv_nsec = (long)(nsec % 1000000000ull);
}

static bool timespec_valid(const struct timespec* ts) {
  if(ts == NULL) {
    return false;
  }
  if(ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000L) {
    return false;
  }
  return true;
}

static bool timespec_to_microseconds(const struct timespec* ts, uint64_t* out_us) {
  if(!timespec_valid(ts) || out_us == NULL) {
    return false;
  }

  if(ts->tv_sec < 0) {
    return false;
  }

  uint64_t sec = (uint64_t)ts->tv_sec;
  if(sec > UINT64_MAX / 1000000ull) {
    return false;
  }

  uint64_t base_us = sec * 1000000ull;
  uint64_t extra_us = ((uint64_t)ts->tv_nsec + 999ull) / 1000ull;

  if(UINT64_MAX - base_us < extra_us) {
    return false;
  }

  *out_us = base_us + extra_us;
  return true;
}

static bool timeval_valid(const struct timeval* tv) {
  if(tv == NULL) {
    return false;
  }
  if(tv->tv_sec < 0) {
    return false;
  }
  if(tv->tv_usec < 0 || tv->tv_usec >= 1000000) {
    return false;
  }
  return true;
}

static bool timeval_to_microseconds(const struct timeval* tv, uint64_t* out_us) {
  if(!timeval_valid(tv) || out_us == NULL) {
    return false;
  }
  uint64_t sec = (uint64_t)tv->tv_sec;
  if(sec > UINT64_MAX / 1000000ull) {
    return false;
  }

  uint64_t base_us = sec * 1000000ull;
  uint64_t extra = (uint64_t)tv->tv_usec;
  if(UINT64_MAX - base_us < extra) {
    return false;
  }

  *out_us = base_us + extra;
  return true;
}

static void microseconds_to_timeval(uint64_t usec, struct timeval* tv) {
  if(tv == NULL) {
    return;
  }
  tv->tv_sec = (time_t)(usec / 1000000ull);
  tv->tv_usec = (suseconds_t)(usec % 1000000ull);
}

static bool copy_user_string(const char* user_ptr, char* dest, size_t capacity) {
  if(current == NULL || user_ptr == NULL || dest == NULL || capacity == 0) {
    return false;
  }

  size_t copied = 0;
  while(copied < capacity) {
    if(!proc_user_buffer_accessible(current, user_ptr + copied, 1)) {
      return false;
    }

    char ch = user_ptr[copied];
    dest[copied++] = ch;
    if(ch == '\0') {
      return true;
    }
  }

  dest[capacity - 1] = '\0';
  return false;
}

static void free_string_vector(char** vector, size_t count) {
  if(vector == NULL) {
    return;
  }
  for(size_t i = 0; i < count; i++) {
    if(vector[i]) {
      kfree(vector[i]);
    }
  }
  kfree(vector);
}

static size_t shm_region_length_bytes(const shm_region_t* region) {
  size_t pages = shm_region_page_count(region);
  return pages * PAGE_SIZE;
}

static virt_addr_t shm_align_down_hint(virt_addr_t addr, int flags) {
  if(flags & SHM_RND) {
    return addr & ~((virt_addr_t)SHMLBA - 1);
  }
  return addr & ~((virt_addr_t)PAGE_SIZE - 1);
}

static virt_addr_t shm_align_up_page(virt_addr_t addr) {
  if((addr & (PAGE_SIZE - 1)) == 0) {
    return addr;
  }
  return (addr + PAGE_SIZE) & ~((virt_addr_t)PAGE_SIZE - 1);
}

static bool shm_range_within_bounds(proc_info_p proc, virt_addr_t base, size_t length) {
  if(proc == NULL) {
    return false;
  }
  virt_addr_t end;
  if(__builtin_add_overflow(base, length, &end)) {
    return false;
  }
  if(base < proc->mmap_base || end > proc->mmap_limit || base >= end) {
    return false;
  }
  return true;
}

static int shm_select_address(proc_info_p proc,
                              size_t length,
                              void* addr_hint,
                              int flags,
                              virt_addr_t* base_out) {
  if(proc == NULL || length == 0 || base_out == NULL) {
    return EINVAL;
  }

  bool hint = (addr_hint != NULL);
  virt_addr_t base;

  if(hint) {
    base = shm_align_down_hint((virt_addr_t)addr_hint, flags);
    if(!shm_range_within_bounds(proc, base, length)) {
      return EINVAL;
    }
    if(vm_range_overlaps(proc, base, length)) {
      return EINVAL;
    }
    *base_out = base;
    return 0;
  }

  base = proc->mmap_next ? proc->mmap_next : proc->mmap_base;
  base = shm_align_up_page(base);

  while(true) {
    virt_addr_t end;
    if(__builtin_add_overflow(base, length, &end)) {
      return ENOMEM;
    }
    if(end > proc->mmap_limit || base < proc->mmap_base || base >= end) {
      return ENOMEM;
    }
    if(!vm_range_overlaps(proc, base, length)) {
      *base_out = base;
      return 0;
    }
    base = shm_align_up_page(end);
  }
}

static char* duplicate_user_string(const char* user_ptr) {
  if(user_ptr == NULL) {
    return NULL;
  }

  size_t capacity = 64;
  char* buffer = kmalloc(capacity);
  if(buffer == NULL) {
    return NULL;
  }

  size_t len = 0;
  while(true) {
    if(len >= EXECVE_MAX_STRING) {
      kfree(buffer);
      return NULL;
    }

    if(!proc_user_buffer_accessible(current, user_ptr + len, 1)) {
      kfree(buffer);
      return NULL;
    }

    char ch = user_ptr[len];
    if(len + 1 >= capacity) {
      size_t new_capacity = capacity * 2;
      char* resized = krealloc(buffer, new_capacity);
      if(resized == NULL) {
        kfree(buffer);
        return NULL;
      }
      buffer = resized;
      capacity = new_capacity;
    }

    buffer[len++] = ch;
    if(ch == '\0') {
      break;
    }
  }

  return buffer;
}

typedef struct {
  char*  user_buffer;
  size_t capacity;
  size_t length;
  int    error;
} listdir_context_t;

static bool listdir_iter_callback(const fs_dir_entry_t* entry, void* context) {
  listdir_context_t* ctx = (listdir_context_t*)context;
  if(ctx == NULL || entry == NULL) {
    return false;
  }

  if(ctx->error != 0) {
    return false;
  }

  size_t name_len = 0;
  while(name_len < sizeof(entry->name) && entry->name[name_len] != '\0') {
    name_len++;
  }
  if(name_len == 0) {
    return true;
  }

  size_t required = name_len + 1; // newline
  if(entry->is_directory) {
    required += 1;
  }

  if(ctx->capacity == 0 || ctx->user_buffer == NULL) {
    ctx->length += required;
    return true;
  }

  if(ctx->length + required > ctx->capacity) {
    ctx->error = -ENOSPC;
    return false;
  }

  if(!proc_user_buffer_accessible(current, ctx->user_buffer + ctx->length, required)) {
    ctx->error = -EFAULT;
    return false;
  }

  char* dest = ctx->user_buffer + ctx->length;
  memcpy(dest, entry->name, name_len);
  size_t offset = name_len;
  if(entry->is_directory) {
    dest[offset++] = '/';
  }
  dest[offset++] = '\n';
  ctx->length += offset;
  return true;
}

static uint64_t syscall_finalize(syscall_frame_t* frame) {
  if(current == NULL) {
    return frame->rax;
  }

  SYSCALL_TRACE("syscall_finalize: entry pid=%u rax=%lx\n",
                current ? current->pid : 0u,
                (unsigned long)frame->rax);

  if(current != NULL && current->pid == 2 && frame != NULL) {
    SYSCALL_TRACE("syscall_finalize frame pid=2 rip=%lx rsp=%lx rdi=%lx rsi=%lx rdx=%lx rcx=%lx\n",
                  (unsigned long)frame->rip,
                  (unsigned long)frame->rsp,
                  (unsigned long)frame->rdi,
                  (unsigned long)frame->rsi,
                  (unsigned long)frame->rdx,
                  (unsigned long)frame->rcx);
  }

#ifndef MENIOS_HOST_TEST
  syscall_last_return_value = frame->rax;
#endif

  if(current != NULL && current->cpu_state != NULL) {
    current->cpu_state->rax = frame->rax;
  }

  proc_signal_delivery_t delivery =
      proc_signal_handle_pending(current, (cpu_state_t*)frame);
  if(delivery == PROC_SIGNAL_DELIVERY_TERMINATED ||
     delivery == PROC_SIGNAL_DELIVERY_STOPPED) {
    frame = (syscall_frame_t*)proc_switch((cpu_state_p)frame);
  }

  SYSCALL_TRACE("syscall_finalize: exit pid=%u rax=%lx delivery=%d\n",
                current ? current->pid : 0u,
                (unsigned long)frame->rax,
                (int)delivery);
  if(current != NULL && current->pid == 2 && frame != NULL) {
    SYSCALL_TRACE("syscall_finalize exit frame pid=2 rip=%lx rsp=%lx rdi=%lx rsi=%lx rdx=%lx rcx=%lx\n",
                  (unsigned long)frame->rip,
                  (unsigned long)frame->rsp,
                  (unsigned long)frame->rdi,
                  (unsigned long)frame->rsi,
                  (unsigned long)frame->rdx,
                  (unsigned long)frame->rcx);
  }
#ifndef MENIOS_HOST_TEST
  SYSCALL_TRACE("syscall_finalize: stored=%lx slot=%lx\n",
                (unsigned long)syscall_last_return_value,
                (unsigned long)syscall_last_return_slot_value);
#endif
  if(current != NULL) {
    current->syscall_gs_active = false;
    current->syscall_gs_needs_restore = false;
  }
  return frame->rax;
}

static bool clone_user_vector(const char* const* user_vec,
                              size_t max_entries,
                              char*** out_vec,
                              size_t* out_count,
                              int* err_out) {
  if(out_vec == NULL || out_count == NULL) {
    return false;
  }

  *out_vec = NULL;
  *out_count = 0;

  if(err_out) {
    *err_out = 0;
  }

  if(user_vec == NULL) {
    return true;
  }

  size_t capacity = 8;
  char** vector = kmalloc(capacity * sizeof(char*));
  if(vector == NULL) {
    return false;
  }

  size_t count = 0;
  bool success = false;

  for(size_t i = 0; i < max_entries; i++) {
    if(!proc_user_buffer_accessible(current, user_vec + i, sizeof(char*))) {
      if(err_out) {
        *err_out = -EFAULT;
      }
      goto out;
    }

    const char* entry = user_vec[i];
    if(entry == NULL) {
      success = true;
      break;
    }

    char* dup = duplicate_user_string(entry);
    if(dup == NULL) {
      if(err_out) {
        *err_out = -EFAULT;
      }
      goto out;
    }

    if(count == capacity) {
      size_t new_capacity = capacity * 2;
      char** resized = krealloc(vector, new_capacity * sizeof(char*));
      if(resized == NULL) {
        kfree(dup);
        if(err_out) {
          *err_out = -ENOMEM;
        }
        goto out;
      }
      vector = resized;
      capacity = new_capacity;
    }

    vector[count++] = dup;
  }

  if(!success) {
    if(proc_user_buffer_accessible(current, user_vec + max_entries, sizeof(char*)) &&
       user_vec[max_entries] == NULL) {
      success = true;
    } else if(err_out) {
      *err_out = -E2BIG;
    }
  }

out:
  if(success) {
    *out_vec = vector;
    *out_count = count;
    return true;
  }

  free_string_vector(vector, count);
  if(err_out && *err_out == 0) {
    *err_out = -EFAULT;
  }
  return false;
}

static void syscall_register(uint64_t number, syscall_handler_t handler) {
  if(number >= SYSCALL_MAX) {
    serial_printf("syscall_register: number %lu out of range\n", number);
    return;
  }
  syscall_table[number] = handler ? handler : syscall_stub_unimplemented;
}

void syscall_init(void) {
  syscall_arch_init();

  for(size_t i = 0; i < SYSCALL_MAX; i++) {
    syscall_table[i] = syscall_stub_unimplemented;
  }

  syscall_register(SYS_READ, syscall_read_handler);
  syscall_register(SYS_WRITE, syscall_write_handler);
  syscall_register(SYS_OPEN, syscall_open_handler);
  syscall_register(SYS_CLOSE, syscall_close_handler);
  syscall_register(SYS_LSEEK, syscall_lseek_handler);
  syscall_register(SYS_MMAP, syscall_mmap_handler);
  syscall_register(SYS_MUNMAP, syscall_munmap_handler);
  syscall_register(SYS_PIPE, syscall_pipe_handler);
  syscall_register(SYS_DUP, syscall_dup_handler);
  syscall_register(SYS_DUP2, syscall_dup2_handler);
  syscall_register(SYS_FORK, syscall_fork_handler);
  syscall_register(SYS_EXECVE, syscall_execve_handler);
  syscall_register(SYS_WAITPID, syscall_waitpid_handler);
  syscall_register(SYS_LISTDIR, syscall_listdir_handler);
  syscall_register(SYS_STDIN_POLL, syscall_stdin_poll_handler);
  syscall_register(SYS_INPUT_EVENT, syscall_input_event_handler);
  syscall_register(SYS_PROC_KILL, syscall_proc_kill_handler);
  syscall_register(SYS_KILL, syscall_kill_handler);
  syscall_register(SYS_SIGACTION, syscall_sigaction_handler);
  syscall_register(SYS_SIGPROCMASK, syscall_sigprocmask_handler);
  syscall_register(SYS_GETSOCKOPT, syscall_getsockopt_handler);
  syscall_register(SYS_PROC_LIST, syscall_proc_list_handler);
  syscall_register(SYS_YIELD, syscall_yield_handler);
  syscall_register(SYS_SLEEP, syscall_sleep_handler);
  syscall_register(SYS_NANOSLEEP, syscall_nanosleep_handler);
  syscall_register(SYS_CLOCK_GETTIME, syscall_clock_gettime_handler);
  syscall_register(SYS_CLOCK_SETTIME, syscall_clock_settime_handler);
  syscall_register(SYS_CLOCK_GETRES, syscall_clock_getres_handler);
  syscall_register(SYS_SETITIMER, syscall_setitimer_handler);
  syscall_register(SYS_GETITIMER, syscall_getitimer_handler);
  syscall_register(SYS_ALARM, syscall_alarm_handler);
  syscall_register(SYS_SIGRETURN, syscall_sigreturn_handler);
  syscall_register(SYS_EXIT, syscall_exit_handler);
  syscall_register(SYS_FCNTL, syscall_fcntl_handler);
  syscall_register(SYS_IOCTL, syscall_ioctl_handler);
  syscall_register(SYS_SHMGET, syscall_shmget_handler);
  syscall_register(SYS_SHMAT, syscall_shmat_handler);
  syscall_register(SYS_SHMDT, syscall_shmdt_handler);
  syscall_register(SYS_SHMCTL, syscall_shmctl_handler);
  syscall_register(SYS_CHDIR, syscall_chdir_handler);
  syscall_register(SYS_GETCWD, syscall_getcwd_handler);
  syscall_register(SYS_UNLINK, syscall_unlink_handler);
  syscall_register(SYS_MKDIR, syscall_mkdir_handler);
  syscall_register(SYS_RMDIR, syscall_rmdir_handler);
  syscall_register(SYS_RENAME, syscall_rename_handler);
  syscall_register(SYS_GETPAGESIZE, syscall_getpagesize_handler);
  syscall_register(SYS_TIME, syscall_time_handler);
  syscall_register(SYS_GETTIMEOFDAY, syscall_gettimeofday_handler);

  serial_printf("syscall_init: dispatcher ready (syscall/sysret)\n");
}

uint64_t syscall_dispatch(syscall_frame_t* frame) {
  uint64_t number = frame->rax;

  if(current != NULL) {
    current->syscall_gs_active = true;
    current->syscall_gs_needs_restore = false;
  }

  SYSCALL_TRACE("syscall_dispatch: pid=%u number=%lu rip=%lx cs=%lx rsp=%lx\n",
                current ? current->pid : 0u,
                number,
                (unsigned long)frame->rip,
                (unsigned long)frame->cs,
                (unsigned long)frame->rsp);

  if(number < SYSCALL_MAX) {
    syscall_handler_t handler = syscall_table[number];
    if(handler) {
      uint64_t result = handler(frame);
      frame->rax = result;
      SYSCALL_TRACE("syscall_dispatch: post-handler rip=%lx cs=%lx rsp=%lx rax=%lx\n",
                    (unsigned long)frame->rip,
                    (unsigned long)frame->cs,
                    (unsigned long)frame->rsp,
                    (unsigned long)frame->rax);
      return syscall_finalize(frame);
    }
  }

  frame->rax = (uint64_t)(-ENOSYS);
  SYSCALL_TRACE("syscall_dispatch: unknown syscall rip=%lx\n",
                (unsigned long)frame->rip);
  return syscall_finalize(frame);
}

static uint64_t syscall_stub_unimplemented(syscall_frame_t* frame) {
  serial_printf("syscall_stub_unimplemented: number %lu not implemented\n", frame->rax);
  return (uint64_t)(-ENOSYS);
}

static uint64_t syscall_read_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  void* buffer = (void*)frame->rsi;
  size_t length = (size_t)frame->rdx;
  file_t* file = proc_file_get(current, fd, NULL);
  if(file == NULL) {
    int err = current->err_no ? current->err_no : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  int64_t result = file_read(file, buffer, length);
  file_unref(file);
  frame->rax = (uint64_t)result;
  return frame->rax;
}

static uint64_t syscall_write_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  const void* buffer = (const void*)frame->rsi;
  size_t length = (size_t)frame->rdx;
#if SYSCALL_TRACE_ENABLED
  size_t sample_len = length < 16 ? length : 16;
  char sample[17];
  if(buffer != NULL && sample_len > 0 && proc_user_buffer_accessible(current, buffer, sample_len)) {
    for(size_t i = 0; i < sample_len; i++) {
      char ch = ((const char*)buffer)[i];
      sample[i] = (ch < ' ' || ch > '~') ? '.' : ch;
    }
    sample[sample_len] = '\0';
  } else {
    sample[0] = '\0';
  }
  SYSCALL_TRACE("write: pid=%u fd=%d len=%lu sample='%s'\n",
                current->pid,
                fd,
                (unsigned long)length,
                sample);
#endif
  file_t* file = proc_file_get(current, fd, NULL);
  if(file == NULL) {
    int err = current->err_no ? current->err_no : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  int64_t result = file_write(file, buffer, length);
  file_unref(file);
  frame->rax = (uint64_t)result;
  return frame->rax;
}

static uint64_t syscall_close_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  int rc = proc_file_close(current, fd);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_open_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* user_path = (const char*)frame->rdi;
  int flags = (int)frame->rsi;
  (void)frame->rdx; // mode currently unused

  if(user_path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char path[SYSCALL_PATH_MAX];
  if(!copy_user_string(user_path, path, sizeof(path))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, path, absolute, sizeof(absolute))) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  uint32_t install_flags = 0;
  if(flags & O_CLOEXEC) {
    install_flags |= FD_FLAG_CLOEXEC;
  }

  file_t* file = NULL;
  int rc = vfs_open(absolute, flags, &file);
  if(rc < 0) {
    SYSCALL_TRACE("syscall_open: pid=%u path=%s flags=0x%x rc=%d\n",
                  current->pid,
                  absolute,
                  flags,
                  rc);
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  int fd = proc_file_install(current, file, install_flags);
  file_unref(file);
  if(fd < 0) {
    frame->rax = (uint64_t)fd;
    return frame->rax;
  }

  frame->rax = (uint64_t)fd;
  return frame->rax;
}

static uint64_t syscall_lseek_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  int64_t offset = (int64_t)frame->rsi;
  int whence = (int)frame->rdx;

  file_t* file = proc_file_get(current, fd, NULL);
  if(file == NULL) {
    int err = current->err_no ? current->err_no : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  int64_t result = file_seek(file, offset, whence);
  file_unref(file);
  frame->rax = (uint64_t)result;
  return frame->rax;
}

static uint64_t syscall_mmap_handler(syscall_frame_t* frame) {
  void* addr = (void*)frame->rdi;
  size_t length = (size_t)frame->rsi;
  int prot = (int)frame->rdx;
  int flags = (int)frame->r10;
  int fd = (int)frame->r8;
  off_t offset = (off_t)frame->r9;

  if((flags & MAP_ANON) != 0 || fd < 0) {
    void* result = kmmap(addr, length, prot, flags, fd, offset);
    if(result == MAP_FAILED) {
      int err = current ? current->err_no : ENOMEM;
      if(err == 0) {
        err = ENOMEM;
      }
      frame->rax = (uint64_t)(-err);
      return frame->rax;
    }

    frame->rax = (uint64_t)result;
    return frame->rax;
  }

  if(current == NULL || !current->user_mode) {
    if(current) {
      current->err_no = ENOSYS;
    }
    frame->rax = (uint64_t)(-ENOSYS);
    return frame->rax;
  }

  file_t* file = proc_file_get(current, fd, NULL);
  if(file == NULL) {
    int err = current->err_no ? current->err_no : EBADF;
    if(current) {
      current->err_no = err;
    }
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  if(file->ops == NULL || file->ops->mmap == NULL) {
    file_unref(file);
    if(current) {
      current->err_no = ENOSYS;
    }
    frame->rax = (uint64_t)(-ENOSYS);
    return frame->rax;
  }

  if((flags & MAP_SHARED) == 0) {
    file_unref(file);
    if(current) {
      current->err_no = EINVAL;
    }
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(offset % PAGE_SIZE != 0) {
    file_unref(file);
    if(current) {
      current->err_no = EINVAL;
    }
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  file_mmap_request_t request = {
    .length = length,
    .offset = offset,
    .prot = prot,
    .flags = flags,
  };

  file_mmap_result_t result;
  int rc = file_mmap(file, &request, &result);
  file_unref(file);
  if(rc < 0) {
    if(current) {
      current->err_no = -rc;
    }
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  if(result.length == 0) {
    if(current) {
      current->err_no = EINVAL;
    }
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if((result.phys_addr & (PAGE_SIZE - 1)) != 0) {
    if(current) {
      current->err_no = EINVAL;
    }
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if((prot & PROT_WRITE) && !result.writable) {
    if(current) {
      current->err_no = EACCES;
    }
    frame->rax = (uint64_t)(-EACCES);
    return frame->rax;
  }

  size_t map_length = result.length;
  if(length != 0 && map_length > length) {
    map_length = length;
  }
  size_t aligned_len = page_align_up_size(map_length);

  virt_addr_t base;
  bool used_hint;
  if(!mmap_select_base(addr, aligned_len, &base, &used_hint)) {
    if(current) {
      current->err_no = ENOMEM;
    }
    frame->rax = (uint64_t)(-ENOMEM);
    return frame->rax;
  }

  uint32_t region_flags = prot_to_region_flags(prot);
  if(!vm_map_physical(current, base, result.phys_addr, aligned_len, region_flags)) {
    if(current) {
      current->err_no = ENOMEM;
    }
    frame->rax = (uint64_t)(-ENOMEM);
    return frame->rax;
  }

  if(!used_hint) {
    virt_addr_t next = page_align_up_addr(base + aligned_len);
    if(next > current->mmap_next) {
      current->mmap_next = next;
    }
  }

  current->err_no = 0;
  frame->rax = (uint64_t)base;
  return frame->rax;
}

static uint64_t syscall_munmap_handler(syscall_frame_t* frame) {
  void* addr = (void*)frame->rdi;
  size_t length = (size_t)frame->rsi;

  int rc = kmunmap(addr, length);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_pipe_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int* fds = (int*)frame->rdi;
  if(fds == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  file_t* read_end = NULL;
  file_t* write_end = NULL;

  int rc = pipe_create(&read_end, &write_end);
  if(rc < 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  int read_fd = proc_file_install(current, read_end, 0);
  if(read_fd < 0) {
    file_unref(read_end);
    file_unref(write_end);
    frame->rax = (uint64_t)read_fd;
    return frame->rax;
  }

  int write_fd = proc_file_install(current, write_end, 0);
  if(write_fd < 0) {
    proc_file_close(current, read_fd);
    file_unref(read_end);
    file_unref(write_end);
    frame->rax = (uint64_t)write_fd;
    return frame->rax;
  }

  file_unref(read_end);
  file_unref(write_end);

  fds[0] = read_fd;
  fds[1] = write_fd;
  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_dup_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int oldfd = (int)frame->rdi;
  int rc = proc_file_dup(current, oldfd, -1, false);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_dup2_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int oldfd = (int)frame->rdi;
  int newfd = (int)frame->rsi;
  int rc = proc_file_dup(current, oldfd, newfd, false);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_fork_handler(syscall_frame_t* frame) {
  int err = 0;
  SYSCALL_TRACE("syscall_fork: pid=%u entering\n", current ? current->pid : 0);
  proc_info_p child = proc_fork(current, frame, &err);
  if(child == NULL) {
    SYSCALL_TRACE("syscall_fork: failure err=%d\n", err);
    if(err == 0) {
      err = -ENOMEM;
    }
    frame->rax = (uint64_t)err;
    return frame->rax;
  }

  frame->rax = (uint64_t)child->pid;
  SYSCALL_TRACE("syscall_fork: returning child pid=%lu\n", frame->rax);
  return frame->rax;
}

static uint64_t syscall_execve_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* user_path = (const char*)frame->rdi;
  if(user_path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char path[SYSCALL_PATH_MAX];
  if(!copy_user_string(user_path, path, sizeof(path))) {
    if(current) {
      current->err_no = EFAULT;
    }
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, path, absolute, sizeof(absolute))) {
    if(current) {
      current->err_no = ENAMETOOLONG;
    }
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  SYSCALL_TRACE("execve: pid=%u path=%s\n", current ? current->pid : 0, absolute);
  SYSCALL_TRACE("execve: frame->rsi=%p frame->rdx=%p\n", (void*)frame->rsi, (void*)frame->rdx);

  char** argv = NULL;
  size_t argc = 0;
  int vector_err = 0;
  if(!clone_user_vector((const char* const*)frame->rsi,
                        EXECVE_MAX_ARGS,
                        &argv,
                        &argc,
                        &vector_err)) {
    SYSCALL_TRACE("execve: clone_user_vector argv failed err=%d\n", vector_err);
    free_string_vector(argv, argc);
    if(current) {
      current->err_no = vector_err ? -vector_err : EFAULT;
    }
    frame->rax = (uint64_t)(vector_err ? vector_err : -EFAULT);
    return frame->rax;
  }

  char** envp = NULL;
  size_t envc = 0;
  if(!clone_user_vector((const char* const*)frame->rdx,
                        EXECVE_MAX_ENVP,
                        &envp,
                        &envc,
                        &vector_err)) {
    SYSCALL_TRACE("execve: clone_user_vector envp failed err=%d\n", vector_err);
    free_string_vector(argv, argc);
    if(current) {
      current->err_no = vector_err ? -vector_err : EFAULT;
    }
    frame->rax = (uint64_t)(vector_err ? vector_err : -EFAULT);
    return frame->rax;
  }
  SYSCALL_TRACE("execve: argc=%lu envc=%lu\n",
                (unsigned long)argc,
                (unsigned long)envc);

  void* image = NULL;
  size_t size = 0;
  if(!vfs_read_all(absolute, &image, &size) || image == NULL || size == 0) {
    SYSCALL_TRACE("execve: vfs_read_all failed size=%lu\n", (unsigned long)size);
    if(image != NULL) {
      kfree(image);
    }
    free_string_vector(argv, argc);
    free_string_vector(envp, envc);
    if(current) {
      current->err_no = ENOENT;
    }
    frame->rax = (uint64_t)(-ENOENT);
    return frame->rax;
  }

  if(size > (32 * 1024 * 1024)) {
    kfree(image);
    free_string_vector(argv, argc);
    free_string_vector(envp, envc);
    if(current) {
      current->err_no = EFBIG;
    }
    frame->rax = (uint64_t)(-EFBIG);
    return frame->rax;
  }

  proc_exec_args_t exec_args = {
    .argc = argc,
    .argv = argv,
    .envc = envc,
    .envp = envp,
  };

  int err = proc_exec_image(current, image, size, frame, &exec_args);
  SYSCALL_TRACE("execve: proc_exec_image err=%d\n", err);
  kfree(image);
  free_string_vector(argv, argc);
  free_string_vector(envp, envc);
  frame->rax = (uint64_t)err;
  return frame->rax;
}

static uint64_t syscall_waitpid_handler(syscall_frame_t* frame) {
  proc_info_p caller = current;
  if(caller == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  SYSCALL_TRACE("waitpid: caller pid=%u pid=%d\n", caller->pid, (int)frame->rdi);

  int pid = (int)frame->rdi;
  int* status_ptr = (int*)frame->rsi;
  int options = (int)frame->rdx;
  bool nonblock = (options & WNOHANG) != 0;

  for(;;) {
    int status = 0;
    int result = proc_waitpid(caller, pid, options, &status);
    SYSCALL_TRACE("waitpid: loop result=%d status=%d\n", result, status);

    if(result > 0) {
      if(status_ptr != NULL) {
        if(!proc_user_buffer_accessible(caller, status_ptr, sizeof(int))) {
          caller->waitpid_waiting = false;
          caller->waitpid_target = -1;
          frame->rax = (uint64_t)(-EFAULT);
          return frame->rax;
        }
        *status_ptr = status;
      }
      caller->waitpid_waiting = false;
      caller->waitpid_target = -1;
      frame->rax = (uint64_t)result;
      return frame->rax;
    }

    if(result < 0) {
      caller->waitpid_waiting = false;
      caller->waitpid_target = -1;
      frame->rax = (uint64_t)result;
      return frame->rax;
    }

    caller->waitpid_waiting = true;
    caller->waitpid_target = (pid <= 0) ? -1 : pid;

    if(nonblock) {
      caller->state = PROC_STATE_WAITING;
      frame->rax = 0;
      return frame->rax;
    }

    caller->state = PROC_STATE_WAITING;
    proc_request_block();
    do {
      frame = (syscall_frame_t*)proc_switch((cpu_state_p)frame);
    } while(current != caller);
    caller->state = PROC_STATE_RUNNING;
    continue;
  }
}

static uint64_t syscall_listdir_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* user_path = (const char*)frame->rdi;
  char* user_buffer = (char*)frame->rsi;
  size_t buffer_size = (size_t)frame->rdx;

  if(user_path == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  char path[SYSCALL_PATH_MAX];
  if(!copy_user_string(user_path, path, sizeof(path))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, path, absolute, sizeof(absolute))) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  bool list_only = (user_buffer == NULL || buffer_size == 0);
  listdir_context_t ctx = {
    .user_buffer = list_only ? NULL : user_buffer,
    .capacity = list_only ? 0 : buffer_size,
    .length = 0,
    .error = 0,
  };

  bool ok = vfs_list(absolute, listdir_iter_callback, &ctx);
  if(!ok && ctx.error == 0) {
    ctx.error = -ENOENT;
  }

  if(ctx.error != 0) {
    frame->rax = (uint64_t)ctx.error;
    return frame->rax;
  }

  if(!list_only && ctx.length < ctx.capacity && proc_user_buffer_accessible(current, user_buffer + ctx.length, 1)) {
    user_buffer[ctx.length] = '\0';
  }

  frame->rax = ctx.length;
  return frame->rax;
}

static uint64_t syscall_chdir_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* user_path = (const char*)frame->rdi;
  if(user_path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char path[SYSCALL_PATH_MAX];
  if(!copy_user_string(user_path, path, sizeof(path))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, path, absolute, sizeof(absolute))) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  if(!vfs_path_is_directory(absolute)) {
    file_t* probe = NULL;
    int rc = vfs_open(absolute, O_RDONLY, &probe);
    if(probe != NULL) {
      file_unref(probe);
    }
    if(rc == 0) {
      frame->rax = (uint64_t)(-ENOTDIR);
      return frame->rax;
    }
    if(rc == -ENOSYS) {
      frame->rax = (uint64_t)(-ENOTDIR);
      return frame->rax;
    }
    frame->rax = (uint64_t)(rc != 0 ? rc : -ENOENT);
    return frame->rax;
  }

  size_t len = strlen(absolute);
  if(len >= PROC_CWD_MAX) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  memcpy(current->cwd, absolute, len + 1);
  current->cwd_len = len;
  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_getcwd_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  char* user_buffer = (char*)frame->rdi;
  size_t size = (size_t)frame->rsi;
  if(user_buffer == NULL || size == 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  size_t length = current->cwd_len ? current->cwd_len : 1;
  size_t needed = length + 1;
  if(needed > size) {
    frame->rax = (uint64_t)(-ERANGE);
    return frame->rax;
  }

  if(!proc_user_buffer_accessible(current, user_buffer, needed)) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  memcpy(user_buffer, current->cwd, needed);
  frame->rax = (uint64_t)user_buffer;
  return frame->rax;
}

static uint64_t syscall_unlink_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* user_path = (const char*)frame->rdi;
  if(user_path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char path[SYSCALL_PATH_MAX];
  if(!copy_user_string(user_path, path, sizeof(path))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, path, absolute, sizeof(absolute))) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  int rc = vfs_unlink(absolute);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_mkdir_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* user_path = (const char*)frame->rdi;
  (void)frame->rsi; // mode currently unused

  if(user_path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char path[SYSCALL_PATH_MAX];
  if(!copy_user_string(user_path, path, sizeof(path))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  if(path[0] == '\0') {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  char absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, path, absolute, sizeof(absolute))) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  int rc = vfs_mkdir(absolute);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_rmdir_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* user_path = (const char*)frame->rdi;
  if(user_path == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char path[SYSCALL_PATH_MAX];
  if(!copy_user_string(user_path, path, sizeof(path))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, path, absolute, sizeof(absolute))) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  int rc = vfs_rmdir(absolute);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_rename_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  const char* old_user = (const char*)frame->rdi;
  const char* new_user = (const char*)frame->rsi;
  if(old_user == NULL || new_user == NULL) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char old_path[SYSCALL_PATH_MAX];
  if(!copy_user_string(old_user, old_path, sizeof(old_path))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char new_path[SYSCALL_PATH_MAX];
  if(!copy_user_string(new_user, new_path, sizeof(new_path))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  char old_absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, old_path, old_absolute, sizeof(old_absolute))) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  char new_absolute[VFS_PATH_MAX];
  if(!vfs_build_absolute_path(current->cwd, new_path, new_absolute, sizeof(new_absolute))) {
    frame->rax = (uint64_t)(-ENAMETOOLONG);
    return frame->rax;
  }

  int rc = vfs_rename(old_absolute, new_absolute);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static char proc_state_code(proc_state_t state) {
  switch(state) {
    case PROC_STATE_RUNNING:
      return 'R';
    case PROC_STATE_READY:
      return 'R';
    case PROC_STATE_WAITING:
      return 'W';
    case PROC_STATE_SLEEPING:
      return 'S';
    case PROC_STATE_ZOMBIE:
      return 'Z';
    case PROC_STATE_NEW:
      return 'I';
    default:
      return '?';
  }
}

static uint64_t syscall_proc_list_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  char* user_buffer = (char*)frame->rdi;
  size_t capacity = (size_t)frame->rsi;

  if(user_buffer == NULL || capacity == 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  char output[(PROC_MAX * 64) + 32];
  size_t out_len = 0;

  const char header[] = "  PID STAT CMD\n";
  size_t header_len = strlen(header);
  if(header_len >= sizeof(output)) {
    frame->rax = (uint64_t)(-ENOSPC);
    return frame->rax;
  }
  memcpy(output + out_len, header, header_len);
  out_len += header_len;

  for(size_t i = 0; i < PROC_MAX; i++) {
    proc_info_p proc = procs[i];
    if(proc == NULL) {
      continue;
    }

    if(proc->state == PROC_STATE_TERMINATED ||
       proc->state == PROC_STATE_ZOMBIE) {
      continue;
    }

    const char* name = proc->name[0] != '\0' ? proc->name : "(unnamed)";
    char state_code = proc_state_code(proc->state);

    char pid_buf[32];
    memset(pid_buf, 0, sizeof(pid_buf));
    lutoa((uint64_t)proc->pid, pid_buf, 10);
    size_t pid_len = strlen(pid_buf);
    size_t name_len = strnlen(name, sizeof(proc->name));

    char line[sizeof(proc->name) + 32];
    size_t line_len = 0;

    size_t pid_field = 5;
    if(pid_len < pid_field) {
      for(size_t pad = 0; pad < pid_field - pid_len; ++pad) {
        line[line_len++] = ' ';
      }
    }
    memcpy(line + line_len, pid_buf, pid_len);
    line_len += pid_len;
    line[line_len++] = ' ';

    for(size_t pad = 0; pad < 3; ++pad) {
      line[line_len++] = ' ';
    }
    line[line_len++] = state_code;
    line[line_len++] = ' ';

    memcpy(line + line_len, name, name_len);
    line_len += name_len;
    line[line_len++] = '\n';
    line[line_len] = '\0';

    if(out_len + line_len >= sizeof(output)) {
      frame->rax = (uint64_t)(-ENOSPC);
      return frame->rax;
    }

    memcpy(output + out_len, line, line_len);
    out_len += line_len;
  }

  if(out_len >= capacity) {
    frame->rax = (uint64_t)(-ENOSPC);
    return frame->rax;
  }

  if(!proc_user_buffer_accessible(current, user_buffer, out_len + 1)) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  memcpy(user_buffer, output, out_len);
  user_buffer[out_len] = '\0';

  frame->rax = out_len;
  return frame->rax;
}

static uint64_t syscall_stdin_poll_handler(syscall_frame_t* frame) {
  (void)frame;
  uint8_t ch = 0;
  if(stdin_try_pop(&ch)) {
    frame->rax = (uint64_t)ch;
  } else {
    frame->rax = (uint64_t)(-EAGAIN);
  }
  return frame->rax;
}

static uint64_t syscall_input_event_handler(syscall_frame_t* frame) {
  menios_key_event_t* user_event = (menios_key_event_t*)frame->rdi;
  if(user_event == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  menios_key_event_t event;
  if(!keyboard_event_try_pop(&event)) {
    frame->rax = (uint64_t)(-EAGAIN);
    return frame->rax;
  }

  if(!proc_user_buffer_accessible(current, user_event, sizeof(event))) {
    keyboard_event_push(&event);
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  if(!proc_user_touch_range(current, (virt_addr_t)user_event, sizeof(event), true)) {
    keyboard_event_push(&event);
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  memcpy(user_event, &event, sizeof(event));
  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_kill_handler(syscall_frame_t* frame) {
  int pid = (int)frame->rdi;
  int signo = (int)frame->rsi;

  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(sigbit(signo) == 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(pid <= 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  proc_info_p target = proc_find_by_pid((uint32_t)pid);
  if(target == NULL) {
    frame->rax = (uint64_t)(-ESRCH);
    return frame->rax;
  }

  if(target == &kernel_process_info) {
    frame->rax = (uint64_t)(-EPERM);
    return frame->rax;
  }

  int rc = proc_signal_send(target, signo);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_sigaction_handler(syscall_frame_t* frame) {
  int signo = (int)frame->rdi;
  const struct sigaction* user_act = (const struct sigaction*)frame->rsi;
  struct sigaction* user_oldact = (struct sigaction*)frame->rdx;

  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  struct sigaction new_action;
  struct sigaction* new_action_ptr = NULL;
  if(user_act != NULL) {
    if(!proc_user_buffer_accessible(current, user_act, sizeof(struct sigaction))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    memcpy(&new_action, user_act, sizeof(struct sigaction));
    new_action_ptr = &new_action;
  }

  struct sigaction old_action;
  struct sigaction* old_action_ptr = (user_oldact != NULL) ? &old_action : NULL;

  int rc = proc_signal_configure_action(current, signo, new_action_ptr, old_action_ptr);
  if(rc != 0) {
    frame->rax = (uint64_t)rc;
    return frame->rax;
  }

  if(user_oldact != NULL) {
    if(!proc_user_buffer_accessible(current, user_oldact, sizeof(struct sigaction))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    memcpy(user_oldact, &old_action, sizeof(struct sigaction));
  }

  frame->rax = 0;
  return 0;
}

static uint64_t syscall_sigprocmask_handler(syscall_frame_t* frame) {
  int how = (int)frame->rdi;
  const sigset_t* user_set = (const sigset_t*)frame->rsi;
  sigset_t* user_oldset = (sigset_t*)frame->rdx;

  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  uint32_t previous_mask = 0;
  int rc = 0;

  if(user_set != NULL) {
    if(!proc_user_buffer_accessible(current, user_set, sizeof(sigset_t))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    sigset_t mask_value = *user_set;
    rc = proc_signal_modify_mask(current, how, (uint32_t)mask_value, &previous_mask);
    if(rc != 0) {
      frame->rax = (uint64_t)rc;
      return frame->rax;
    }
  } else {
    previous_mask = current->signal_blocked;
  }

  if(user_oldset != NULL) {
    if(!proc_user_buffer_accessible(current, user_oldset, sizeof(sigset_t))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    *user_oldset = (sigset_t)previous_mask;
  }

  frame->rax = 0;
  return 0;
}

static uint64_t syscall_sigreturn_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  virt_addr_t user_frame_addr = (virt_addr_t)frame->rdi;
  if(user_frame_addr == 0) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  menios_signal_frame_t user_frame;
  if(!proc_user_copy_in(current, &user_frame, user_frame_addr, sizeof(user_frame))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  proc_signal_set_blocked(current, (uint32_t)user_frame.signal_mask);

#if SYSCALL_TRACE_ENABLED
  syscall_frame_t context_copy = user_frame.context;
  serial_printf("sigreturn: frame_ptr=%lx rip=%lx rax=%lx rflags=%lx rsp=%lx cs=%x ss=%x signo=%d\n",
                user_frame_addr,
                context_copy.rip,
                context_copy.rax,
                context_copy.rflags,
                context_copy.rsp,
                (unsigned int)context_copy.cs,
                (unsigned int)context_copy.ss,
                user_frame.signo);

  uint64_t ctx_words[sizeof(context_copy) / sizeof(uint64_t)];
  memcpy(ctx_words, &context_copy, sizeof(context_copy));
  serial_printf("frame words: %lx %lx %lx %lx %lx %lx\n",
                ctx_words[14], ctx_words[15], ctx_words[16], ctx_words[17], ctx_words[18], ctx_words[19]);
#endif

  memcpy(frame, &user_frame.context, sizeof(user_frame.context));

  return frame->rax;
}

static uint64_t syscall_proc_kill_handler(syscall_frame_t* frame) {
  uint32_t pid = (uint32_t)frame->rdi;
  int code = (int)frame->rsi;
  int rc = proc_kill_pid(pid, code);
  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_yield_handler(syscall_frame_t* frame) {
  proc_info_p caller = current;
  proc_request_yield();
  do {
    frame = (syscall_frame_t*)proc_switch((cpu_state_p)frame);
  } while(current != caller);
  frame->rax = 0;
  return 0;
}

static uint64_t syscall_sleep_handler(syscall_frame_t* frame) {
  uint64_t usec = frame->rdi;
  proc_info_p caller = current;
  proc_request_sleep(usec);
  do {
    frame = (syscall_frame_t*)proc_switch((cpu_state_p)frame);
  } while(current != caller);
  frame->rax = 0;
  return 0;
}

static uint64_t syscall_nanosleep_handler(syscall_frame_t* frame) {
  const struct timespec* user_req = (const struct timespec*)frame->rdi;
  struct timespec* user_rem = (struct timespec*)frame->rsi;

  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(user_req == NULL ||
     !proc_user_buffer_accessible(current, user_req, sizeof(struct timespec))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  struct timespec req;
  memcpy(&req, user_req, sizeof(req));

  if(req.tv_sec < 0 || req.tv_nsec < 0 || req.tv_nsec >= 1000000000L) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(req.tv_sec == 0 && req.tv_nsec == 0) {
    if(user_rem != NULL &&
       proc_user_buffer_accessible(current, user_rem, sizeof(struct timespec))) {
      struct timespec zero = {0, 0};
      memcpy(user_rem, &zero, sizeof(zero));
    }
    frame->rax = 0;
    return 0;
  }

  uint64_t sec = (uint64_t)req.tv_sec;
  if(sec > UINT64_MAX / 1000000ull) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  uint64_t duration_us = sec * 1000000ull;
  uint64_t extra_us = ((uint64_t)req.tv_nsec + 999ull) / 1000ull;
  if(extra_us == 0) {
    extra_us = (req.tv_nsec == 0) ? 0 : 1;
  }
  if(UINT64_MAX - duration_us < extra_us) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }
  duration_us += extra_us;
  uint64_t start_us = unix_time_us();
  uint64_t target_us = (UINT64_MAX - duration_us < start_us)
                         ? UINT64_MAX
                         : start_us + duration_us;

  proc_info_p caller = current;
  proc_request_sleep(duration_us);

  do {
    frame = (syscall_frame_t*)proc_switch((cpu_state_p)frame);
  } while(current != caller);

  uint64_t end_us = unix_time_us();
  uint64_t remaining_us = (target_us > end_us) ? (target_us - end_us) : 0;

  if(remaining_us > 0) {
    if(user_rem != NULL) {
      if(!proc_user_buffer_accessible(current, user_rem, sizeof(struct timespec))) {
        frame->rax = (uint64_t)(-EFAULT);
        return frame->rax;
      }
      struct timespec rem;
      rem.tv_sec = (time_t)(remaining_us / 1000000ull);
      rem.tv_nsec = (long)((remaining_us % 1000000ull) * 1000ull);
      memcpy(user_rem, &rem, sizeof(rem));
    }
    frame->rax = (uint64_t)(-EINTR);
    return frame->rax;
  }

  if(user_rem != NULL) {
    if(!proc_user_buffer_accessible(current, user_rem, sizeof(struct timespec))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    struct timespec zero = {0, 0};
    memcpy(user_rem, &zero, sizeof(zero));
  }

  frame->rax = 0;
  return 0;
}

static uint64_t syscall_clock_gettime_handler(syscall_frame_t* frame) {
  clockid_t clk_id = (clockid_t)frame->rdi;
  struct timespec* user_tp = (struct timespec*)frame->rsi;

  if(user_tp == NULL ||
     !proc_user_buffer_accessible(current, user_tp, sizeof(struct timespec))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  struct timespec ts;

  switch(clk_id) {
    case CLOCK_REALTIME: {
      uint64_t now_us = realtime_now_us();
      fill_timespec_from_us(&ts, now_us);
      break;
    }
    case CLOCK_MONOTONIC: {
      uint64_t ns = (uint64_t)ns_from_boot();
      fill_timespec_from_ns(&ts, ns);
      break;
    }
    default:
      frame->rax = (uint64_t)(-EINVAL);
      return frame->rax;
  }

  memcpy(user_tp, &ts, sizeof(ts));
  frame->rax = 0;
  return 0;
}

static uint64_t syscall_clock_settime_handler(syscall_frame_t* frame) {
  clockid_t clk_id = (clockid_t)frame->rdi;
  const struct timespec* user_tp = (const struct timespec*)frame->rsi;

  if(clk_id != CLOCK_REALTIME) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(user_tp == NULL ||
     !proc_user_buffer_accessible(current, user_tp, sizeof(struct timespec))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  struct timespec ts;
  memcpy(&ts, user_tp, sizeof(ts));

  uint64_t desired_us;
  if(!timespec_to_microseconds(&ts, &desired_us)) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  uint64_t now_us = unix_time_us();
  int64_t delta = (int64_t)desired_us - (int64_t)now_us;
  realtime_offset_us = delta;

  frame->rax = 0;
  return 0;
}

static uint64_t syscall_clock_getres_handler(syscall_frame_t* frame) {
  clockid_t clk_id = (clockid_t)frame->rdi;
  struct timespec* user_res = (struct timespec*)frame->rsi;

  if(user_res == NULL ||
     !proc_user_buffer_accessible(current, user_res, sizeof(struct timespec))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  struct timespec ts;

  switch(clk_id) {
    case CLOCK_REALTIME:
    case CLOCK_MONOTONIC:
      ts.tv_sec = 0;
      ts.tv_nsec = 1000; // 1 microsecond resolution
      break;
    default:
      frame->rax = (uint64_t)(-EINVAL);
      return frame->rax;
  }

  memcpy(user_res, &ts, sizeof(ts));
  frame->rax = 0;
  return 0;
}

static proc_itimer_t* proc_get_itimer(proc_info_p proc, int which) {
  if(proc == NULL) {
    return NULL;
  }

  switch(which) {
    case ITIMER_REAL:
      return &proc->timers[PROC_ITIMER_REAL];
    default:
      return NULL;
  }
}

static uint64_t syscall_setitimer_handler(syscall_frame_t* frame) {
  int which = (int)frame->rdi;
  const struct itimerval* user_new = (const struct itimerval*)frame->rsi;
  struct itimerval* user_old = (struct itimerval*)frame->rdx;

  proc_info_p proc = current;
  if(proc == NULL || !proc->user_mode) {
    frame->rax = (uint64_t)(-ENOSYS);
    return frame->rax;
  }

  proc_itimer_t* timer = proc_get_itimer(proc, which);
  if(timer == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  struct itimerval new_value;
  if(user_new == NULL ||
     !proc_user_buffer_accessible(proc, user_new, sizeof(struct itimerval))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }
  memcpy(&new_value, user_new, sizeof(new_value));

  uint64_t now = realtime_now_us();

  if(user_old != NULL) {
    if(!proc_user_buffer_accessible(proc, user_old, sizeof(struct itimerval))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }

    struct itimerval old_value;
    if(timer->active && timer->expires_us > now) {
      uint64_t remaining = timer->expires_us - now;
      microseconds_to_timeval(remaining, &old_value.it_value);
    } else {
      old_value.it_value.tv_sec = 0;
      old_value.it_value.tv_usec = 0;
    }
    microseconds_to_timeval(timer->interval_us, &old_value.it_interval);
    memcpy(user_old, &old_value, sizeof(old_value));
  }

  uint64_t value_us = 0;
  uint64_t interval_us = 0;

  if(!timeval_to_microseconds(&new_value.it_value, &value_us)) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }
  if(!timeval_to_microseconds(&new_value.it_interval, &interval_us)) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(value_us == 0) {
    timer->active = false;
    timer->interval_us = interval_us;
    timer->expires_us = 0;
    frame->rax = 0;
    return 0;
  }

  timer->interval_us = interval_us;
  timer->active = true;

  if(UINT64_MAX - now <= value_us) {
    timer->expires_us = UINT64_MAX;
  } else {
    timer->expires_us = now + value_us;
  }

  frame->rax = 0;
  return 0;
}

static uint64_t syscall_getitimer_handler(syscall_frame_t* frame) {
  int which = (int)frame->rdi;
  struct itimerval* user_value = (struct itimerval*)frame->rsi;

  proc_info_p proc = current;
  if(proc == NULL || !proc->user_mode) {
    frame->rax = (uint64_t)(-ENOSYS);
    return frame->rax;
  }

  if(user_value == NULL ||
     !proc_user_buffer_accessible(proc, user_value, sizeof(struct itimerval))) {
    frame->rax = (uint64_t)(-EFAULT);
    return frame->rax;
  }

  proc_itimer_t* timer = proc_get_itimer(proc, which);
  if(timer == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  uint64_t now = realtime_now_us();
  struct itimerval value;

  if(timer->active && timer->expires_us > now) {
    uint64_t remaining = timer->expires_us - now;
    microseconds_to_timeval(remaining, &value.it_value);
  } else {
    value.it_value.tv_sec = 0;
    value.it_value.tv_usec = 0;
  }

  microseconds_to_timeval(timer->interval_us, &value.it_interval);
  memcpy(user_value, &value, sizeof(value));

  frame->rax = 0;
  return 0;
}

static uint64_t syscall_alarm_handler(syscall_frame_t* frame) {
  unsigned int seconds = (unsigned int)frame->rdi;

  proc_info_p proc = current;
  if(proc == NULL || !proc->user_mode) {
    frame->rax = (uint64_t)(-ENOSYS);
    return frame->rax;
  }

  proc_itimer_t* timer = proc_get_itimer(proc, ITIMER_REAL);
  if(timer == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  uint64_t now = realtime_now_us();
  uint64_t remaining_seconds = 0;

  if(timer->active && timer->expires_us > now) {
    uint64_t delta = timer->expires_us - now;
    remaining_seconds = delta / 1000000ull;
    if((delta % 1000000ull) != 0) {
      remaining_seconds++;
    }
    if(remaining_seconds > UINT_MAX) {
      remaining_seconds = UINT_MAX;
    }
  }

  if(seconds == 0) {
    timer->active = false;
    timer->interval_us = 0;
    timer->expires_us = 0;
    frame->rax = (uint64_t)((unsigned int)remaining_seconds);
    return frame->rax;
  }

  uint64_t value_us = (uint64_t)seconds * 1000000ull;

  timer->interval_us = 0;
  timer->active = true;

  if(UINT64_MAX - now <= value_us) {
    timer->expires_us = UINT64_MAX;
  } else {
    timer->expires_us = now + value_us;
  }

  frame->rax = (uint64_t)((unsigned int)remaining_seconds);
  return frame->rax;
}

static uint64_t syscall_exit_handler(syscall_frame_t* frame) {
  int status = (int)frame->rdi;

  proc_exit(status);

  syscall_frame_t* resumed =
      (syscall_frame_t*)proc_switch((cpu_state_p)frame);

  serial_printf("sys_exit: proc_switch returned frame=%p current_pid=%u\n",
                (void*)resumed,
                current ? current->pid : 0u);

  if(resumed == NULL) {
    halt();
  }

  if(resumed != frame) {
    serial_printf("sys_exit: copying resumed frame from %p to %p size=%zu\n",
                  (void*)resumed,
                  (void*)frame,
                  sizeof(syscall_frame_t));
    memcpy(frame, resumed, sizeof(syscall_frame_t));
  }

  return frame->rax;
}

static uint64_t syscall_fcntl_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  int cmd = (int)frame->rsi;
  uint64_t arg = frame->rdx;

  uint32_t fd_flags = 0;
  file_t* file = proc_file_get(current, fd, &fd_flags);
  if(file == NULL) {
    int err = current->err_no ? current->err_no : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  file_unref(file);

  switch(cmd) {
    case F_GETFD: {
      int result = (fd_flags & FD_FLAG_CLOEXEC) ? FD_CLOEXEC : 0;
      frame->rax = (uint64_t)result;
      return frame->rax;
    }
    case F_SETFD: {
      uint32_t new_flags = fd_flags;
      if(arg & FD_CLOEXEC) {
        new_flags |= FD_FLAG_CLOEXEC;
      } else {
        new_flags &= ~FD_FLAG_CLOEXEC;
      }
      int rc = proc_file_set_flags(current, fd, new_flags);
      frame->rax = (uint64_t)rc;
      return frame->rax;
    }
    default:
      frame->rax = (uint64_t)(-ENOSYS);
      return frame->rax;
  }
}

static uint64_t syscall_ioctl_handler(syscall_frame_t* frame) {
  if(current == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  int fd = (int)frame->rdi;
  unsigned long request = (unsigned long)frame->rsi;
  void* argp = (void*)frame->rdx;

  file_t* file = proc_file_get(current, fd, NULL);
  if(file == NULL) {
    int err = current->err_no ? current->err_no : EBADF;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  int rc = file_ioctl(file, request, argp);
  file_unref(file);

  frame->rax = (uint64_t)rc;
  return frame->rax;
}

static uint64_t syscall_shmget_handler(syscall_frame_t* frame) {
  shm_key_t key = (shm_key_t)frame->rdi;
  size_t size = (size_t)frame->rsi;
  int shmflg = (int)frame->rdx;

  if(size == 0 && key == IPC_PRIVATE) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(size == 0) {
    size = 1;
  }

  uint16_t mode = (uint16_t)(shmflg & 0x1FF);
  bool create = (shmflg & IPC_CREAT) != 0;
  bool exclusive = create && ((shmflg & IPC_EXCL) != 0);

  if(key != IPC_PRIVATE) {
    shm_region_t* existing = shm_region_get_by_key(key);
    if(existing != NULL) {
      if(exclusive) {
        shm_region_unref(existing);
        frame->rax = (uint64_t)(-EEXIST);
        return frame->rax;
      }
      if(create && size > shm_region_size(existing)) {
        shm_region_unref(existing);
        frame->rax = (uint64_t)(-EINVAL);
        return frame->rax;
      }
      int id = shm_region_id(existing);
      shm_region_unref(existing);
      frame->rax = (uint64_t)id;
      return frame->rax;
    }

    if(!create) {
      frame->rax = (uint64_t)(-ENOENT);
      return frame->rax;
    }
  }

  size_t aligned = (size + PAGE_SIZE - 1) & ~((size_t)PAGE_SIZE - 1);
  if(aligned == 0) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  shm_region_create_status_t status;
  shm_region_t* region = shm_region_create(key, aligned, mode, NULL, &status);
  if(region == NULL) {
    int err = (status == SHM_REGION_CREATE_INVALID_ARGUMENT) ? EINVAL : ENOMEM;
    frame->rax = (uint64_t)(-err);
    return frame->rax;
  }

  frame->rax = (uint64_t)shm_region_id(region);
  return frame->rax;
}

static uint64_t syscall_shmat_handler(syscall_frame_t* frame) {
  if(current == NULL || !current->user_mode) {
    frame->rax = (uint64_t)(-ENOSYS);
    return frame->rax;
  }

  int shmid = (int)frame->rdi;
  void* shmaddr = (void*)frame->rsi;
  int shmflg = (int)frame->rdx;

  if(shmflg & SHM_REMAP) {
    frame->rax = (uint64_t)(-ENOSYS);
    return frame->rax;
  }

  shm_region_t* region = shm_region_get_by_id(shmid);
  if(region == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  if(shm_region_marked_for_removal(region)) {
    shm_region_unref(region);
    frame->rax = (uint64_t)(-EIDRM);
    return frame->rax;
  }

  size_t length = shm_region_length_bytes(region);
  if(length == 0) {
    shm_region_unref(region);
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  bool writable = (shmflg & SHM_RDONLY) == 0;
  uint32_t flags = VM_REGION_FLAG_USER | VM_REGION_FLAG_READ;
  if(writable) {
    flags |= VM_REGION_FLAG_WRITE;
  }

  virt_addr_t base;
  int sel_rc = shm_select_address(current, length, shmaddr, shmflg, &base);
  if(sel_rc != 0) {
    shm_region_unref(region);
    frame->rax = (uint64_t)(-sel_rc);
    return frame->rax;
  }

  if(!vm_map_shared(current, region, base, flags, writable)) {
    shm_region_unref(region);
    frame->rax = (uint64_t)(-ENOMEM);
    return frame->rax;
  }

  if(!proc_shm_track_attachment(current, region, base, length, shmid, shmflg)) {
    vm_unmap(current, base, length);
    shm_region_unref(region);
    frame->rax = (uint64_t)(-ENOSPC);
    return frame->rax;
  }

  virt_addr_t end = base + length;
  virt_addr_t next = shm_align_up_page(end);
  if(next > current->mmap_next) {
    current->mmap_next = next;
  }

  frame->rax = (uint64_t)base;
  return frame->rax;
}

static uint64_t syscall_shmdt_handler(syscall_frame_t* frame) {
  if(current == NULL || !current->user_mode) {
    frame->rax = (uint64_t)(-ENOSYS);
    return frame->rax;
  }

  void* addr = (void*)frame->rdi;
  if(addr == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  virt_addr_t base = (virt_addr_t)addr;
  base = shm_align_down_hint(base, 0);

  if(!proc_shm_detach(current, base)) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  frame->rax = 0;
  return frame->rax;
}

static uint64_t syscall_shmctl_handler(syscall_frame_t* frame) {
  int shmid = (int)frame->rdi;
  int cmd = (int)frame->rsi;
  struct shmid_ds* user_buf = (struct shmid_ds*)frame->rdx;

  shm_region_t* region = shm_region_get_by_id(shmid);
  if(region == NULL) {
    frame->rax = (uint64_t)(-EINVAL);
    return frame->rax;
  }

  uint64_t result = 0;

  switch(cmd) {
    case IPC_RMID:
      shm_region_set_marked_for_removal(region, true);
      break;
    case IPC_STAT: {
      if(user_buf == NULL ||
         !proc_user_buffer_accessible(current, user_buf, sizeof(struct shmid_ds))) {
        result = (uint64_t)(-EFAULT);
        break;
      }

      struct shmid_ds info;
      memset(&info, 0, sizeof(info));
      info.shm_perm.key = shm_region_key(region);
      info.shm_perm.mode = shm_region_mode(region);
      info.shm_segsz = shm_region_size(region);
      info.shm_nattch = (unsigned short)shm_region_attachment_count(region);
      memcpy(user_buf, &info, sizeof(info));
      break;
    }
    default:
      result = (uint64_t)(-ENOSYS);
      break;
  }

  shm_region_unref(region);
  frame->rax = result;
  return frame->rax;
}

static uint64_t syscall_getsockopt_handler(syscall_frame_t* frame) {
  (void)frame;
  if(current != NULL) {
    current->err_no = ENOSYS;
  }
  frame->rax = (uint64_t)(-ENOSYS);
  return frame->rax;
}

static uint64_t syscall_getpagesize_handler(syscall_frame_t* frame) {
  (void)frame;
  return (uint64_t)PAGE_SIZE;
}

static uint64_t syscall_time_handler(syscall_frame_t* frame) {
  time_t* user_ptr = (time_t*)frame->rdi;
  uint64_t now_us = realtime_now_us();
  time_t now = (time_t)(now_us / 1000000ull);

  if(user_ptr != NULL) {
    if(!proc_user_buffer_accessible(current, user_ptr, sizeof(time_t))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
    *user_ptr = now;
  }

  frame->rax = (uint64_t)now;
  return frame->rax;
}

static uint64_t syscall_gettimeofday_handler(syscall_frame_t* frame) {
  struct timeval* tv = (struct timeval*)frame->rdi;
  struct timezone* tz = (struct timezone*)frame->rsi;

  if(tv != NULL) {
    if(!proc_user_buffer_accessible(current, tv, sizeof(struct timeval))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
  }

  if(tz != NULL) {
    if(!proc_user_buffer_accessible(current, tz, sizeof(struct timezone))) {
      frame->rax = (uint64_t)(-EFAULT);
      return frame->rax;
    }
  }

  uint64_t now_us = realtime_now_us();

  if(tv != NULL) {
    tv->tv_sec = (time_t)(now_us / 1000000ull);
    tv->tv_usec = (suseconds_t)(now_us % 1000000ull);
  }

  if(tz != NULL) {
    tz->tz_minuteswest = 0;
    tz->tz_dsttime = 0;
  }

  frame->rax = 0;
  return frame->rax;
}
