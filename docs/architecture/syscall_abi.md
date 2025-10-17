# meniOS System Call ABI

meniOS exposes a 64-bit system call interface for userland programs.  This document
summarises the calling convention, error handling contract, and the catalog of
currently implemented syscalls so the toolchain and libc work can rely on a
stable specification.

## Entry mechanism

- **Fast path**: user mode executes the `syscall` instruction. The MSR-programmed
  entry stub (`src/kernel/lidt.s:sym=syscall_entry`) switches to the per-CPU
  kernel stack, materialises a `syscall_frame_t`, and tail-calls
  `syscall_dispatch`. The return path properly preserves full 64-bit return values.
- **Compatibility**: the legacy `int $0x80` gate remains in the IDT for
  debugging, but production binaries should use the wrappers in
  `include/menios/syscall_user.h` which emit `syscall` directly.
- **CPU mode**: long mode (x86-64). All arguments and return values are 64-bit.

### Register usage

| Register | On entry | On return |
| -------- | -------- | --------- |
| `rax`    | System call number (see `include/menios/syscall.h`). | Return value (non-negative success, negative `-errno` on failure). |
| `rdi`    | Argument 0 | Preserved unless documented otherwise. |
| `rsi`    | Argument 1 | Preserved. |
| `rdx`    | Argument 2 | Preserved. |
| `r10`    | Argument 3 | Preserved. |
| `r8`     | Argument 4 | Preserved. |
| `r9`     | Argument 5 | Preserved. |
| `rcx`, `r11` | Used internally by `syscall`; always destroyed. |
| Stack pointer | Must remain 16-byte aligned.  The kernel neither adjusts nor validates the user stack. |

The inline helpers in `include/menios/syscall_user.h` already follow this
convention.  For calls with four or more parameters (for example `mmap`) you must
place arguments manually in `r10`, `r8`, and `r9`, mirroring Linux.

### Error reporting

The kernel returns non-negative values on success.  Failures are reported as a
negative errno (`-E...`) stored in `rax`.  Userland helpers typically convert
negative results into `errno` and return `-1`.  No errno values are written by
the kernel directly into user memory.

The errno namespace is defined in `include/sys/errno.h` and is intentionally
aligned with POSIX where possible.

### Pointer and string handling rules

- Paths copied from user space are limited to `SYSCALL_PATH_MAX` (256 bytes).
  They are resolved relative to the caller's `cwd` using
  `vfs_build_absolute_path`.
- `execve` copies up to 64 arguments and 64 environment variables and limits the
  total copied string data to 4096 bytes.  Excess arguments or environment
  values yield `-E2BIG`.
- All buffers provided by userland are validated through
  `proc_user_buffer_accessible` before the kernel reads or writes them.  If the
  check fails the kernel returns `-EFAULT`.

## System call reference

System call numbers are listed in `include/menios/syscall.h`.  They currently
mirror Linux values for the overlapping subset but that is not a compatibility
promise.  Below is a summary of the calls that ship in meniOS v0.1.0.

### File descriptor I/O

- **`SYS_READ` (0)** — `ssize_t read(int fd, void *buf, size_t count);`
  Reads from an open file descriptor.  Returns the number of bytes read or
  `-EBADF`, `-EFAULT`, or storage-driver specific errors.
- **`SYS_WRITE` (1)** — `ssize_t write(int fd, const void *buf, size_t count);`
  Writes to an open descriptor.  Returns the number of bytes written or
  negative errno on failure.
- **`SYS_OPEN` (2)** — `int open(const char *path, int flags /*mode unused*/);`
  Opens a file relative to the caller's `cwd`.  `O_CLOEXEC` is honoured at file
  installation; the mode parameter is currently ignored.  Returns a descriptor
  number.
- **`SYS_CLOSE` (3)** — `int close(int fd);`
  Closes a descriptor.
- **`SYS_LSEEK` (8)** — `off_t lseek(int fd, int64_t offset, int whence);`
  Adjusts the file position.
- **`SYS_DUP` (32)** / **`SYS_DUP2` (33)** — `int dup(int oldfd);`,
  `int dup2(int oldfd, int newfd);`
  Duplicate file descriptors.  `dup2` will overwrite the target descriptor.
- **`SYS_PIPE` (22)** — `int pipe(int fds[2]);`
  Creates a unidirectional pipe.  On success `fds[0]` is readable and `fds[1]`
  is writable.
- **`SYS_FCNTL` (72)** — `int fcntl(int fd, int cmd, unsigned long arg);`
  Currently supports `F_GETFD` and `F_SETFD` for the `FD_CLOEXEC` flag.
- **`SYS_IOCTL` (73)** — `int ioctl(int fd, unsigned long request, void *argp);`
  Forwards to the underlying driver.  Only the device filesystem exposes
  handlers today; unsupported requests return `-ENOTTY` or driver-specific
  errors.

### Memory management

- **`SYS_MMAP` (9)** —
  `void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);`
  Currently limited to `MAP_ANONYMOUS | MAP_PRIVATE` mappings.  File-backed
  mappings and `MAP_FIXED` are not yet supported.  Memory is allocated in page
  sized chunks and aligned to the userland mmap window.
- **`SYS_MUNMAP` (11)** — `int munmap(void *addr, size_t length);`
  Unmaps a region previously returned by `mmap`.
- **`SYS_SHMGET` (74)** / **`SYS_SHMAT` (75)** / **`SYS_SHMDT` (76)** /
  **`SYS_SHMCTL` (77)** — System V shared memory primitives.  `SHM_REMAP` is
  rejected, and only `IPC_RMID`/`IPC_STAT` are implemented in `shmctl`.

### Process management

- **`SYS_FORK` (57)** — `pid_t fork(void);`
  Parent receives the child's PID, the child sees zero, and failures return
  `-ENOMEM` or another negative errno.
- **`SYS_EXECVE` (59)** — `int execve(const char *path, char *const argv[], char *const envp[]);`
  Replaces the current image.  Path resolution honours the caller's `cwd`.
- **`SYS_EXIT` (60)** — `void _exit(int status);`
  Terminates the calling process.  Does not return.
- **`SYS_WAITPID` (61)** — `pid_t waitpid(pid_t pid, int *status, int options);`
  Supports `WNOHANG`.  On success writes a status code that follows traditional
  POSIX `waitpid` encoding.
- **`SYS_PROC_KILL` (64)** — `int proc_kill(pid_t pid, int status);`
  Forcefully transitions the target process to zombie state and records the
  provided status code.  Used by init's supervision loop.
- **`SYS_PROC_LIST` (65)** — `ssize_t proc_list(char *buffer, size_t capacity);`
  Writes a newline-delimited snapshot of active processes beginning with the
  header `PID STATE NAME`.  Requires enough capacity to include a trailing NUL.
- **`SYS_YIELD` (24)** — `int sched_yield(void);`
  Marks the current thread ready and switches to the scheduler.  Always returns
  zero after rescheduling.
- **`SYS_SLEEP` (35)** — `int usleep(uint64_t usec);`
  Parks the caller for at least the requested microseconds (rounded up to the
  scheduler tick).  Always returns zero and is not interruptible by signals.
- **`SYS_NANOSLEEP` (83)** —
  `int nanosleep(const struct timespec *req, struct timespec *rem);`
  Suspends the caller for the requested duration with microsecond resolution.
  Returns zero on success.  If interrupted by a signal, writes the remaining
  time to `rem` (when non-NULL) and returns `-EINTR`.

### Signals

- **`SYS_KILL` (66)** — `int kill(pid_t pid, int signo);`
  Delivers a signal to another process.  The kernel validates the signal number
  and process state.
- **`SYS_SIGACTION` (67)** — `int sigaction(int signo, const struct sigaction *act, struct sigaction *oldact);`
  Copies handler state between userland and the kernel.  Handlers execute on the
  user stack.
- **`SYS_SIGPROCMASK` (68)** — `int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);`
  Adjusts or queries the calling process's signal mask.

### Filesystem utilities

- **`SYS_LISTDIR` (62)** —
  `ssize_t listdir(const char *path, char *buffer, size_t size);`
  Enumerates directory entries into a newline-delimited string.  If `buffer` is
  `NULL` or `size` is zero the call returns the number of bytes that would have
  been written.
- **`SYS_CHDIR` (78)** — `int chdir(const char *path);`
  Updates the process working directory.
- **`SYS_GETCWD` (79)** — `char *getcwd(char *buf, size_t size);`
  Copies the current working directory into user memory.  The return value is
  the provided buffer pointer on success.

### Terminal helpers

- **`SYS_STDIN_POLL` (63)** — `int stdin_poll(void);`
  Non-blocking read from the kernel's keyboard buffer.  Returns the next byte
  (0–255) or `-EAGAIN` if no input is waiting.  Used by `mosh`'s interactive
  loop.

### Time and date

- **`SYS_TIME` (81)** — `time_t time(time_t *tloc);`
  Returns the current time as seconds since the Unix epoch (January 1, 1970).
  If `tloc` is non-NULL and accessible, the time is also stored at that address.
  Returns the time value or `-EFAULT` if `tloc` is invalid.
- **`SYS_GETTIMEOFDAY` (82)** — `int gettimeofday(struct timeval *tv, struct timezone *tz);`
  Retrieves the current time with microsecond precision.  The `tv` parameter
  receives seconds and microseconds since the Unix epoch.  The `tz` parameter
  is ignored (for POSIX compatibility).  Returns zero on success or `-EFAULT`
  if `tv` is NULL or inaccessible.
- **`SYS_GETPAGESIZE` (80)** — `long getpagesize(void);`
  Returns the system page size (typically 4096 bytes).  Used by the userland
  allocator and other memory management utilities.

### IPC status

Beyond shared memory the kernel does not yet expose sockets, message queues, or
other IPC surfaces.  Future milestones (issues #105–#107) will extend the ABI;
for now userland should rely on pipes, signals, shared memory, and the
supervision helpers documented above.

## Example: making a six-argument call

```c
#include <menios/syscall.h>

void *anon_page(void) {
  register long rax asm("rax") = SYS_MMAP;
  register long rdi asm("rdi") = 0;                     // hint address
  register long rsi asm("rsi") = 4096;                  // length
  register long rdx asm("rdx") = PROT_READ | PROT_WRITE;
  register long r10 asm("r10") = MAP_PRIVATE | MAP_ANONYMOUS;
  register long r8  asm("r8")  = -1;                    // fd (ignored)
  register long r9  asm("r9")  = 0;                     // offset

  asm volatile("syscall"
               : "+a"(rax)
               : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9)
               : "rcx", "r11", "memory");

  if(rax < 0) {
    // errno style handling here
    return (void*)-1;
  }
  return (void*)rax;
}
```

## Compatibility notes

- The ABI intentionally mimics Linux where practical, but only the calls listed
  above are guaranteed to exist.  Expect differences in flag handling and error
  coverage.
- **Implementation status**: The `syscall` instruction is fully implemented with
  proper 64-bit return value handling (#221, #274 complete).
- Be conservative with path lengths and buffer sizes.  Exceeding the documented
  limits results in `-ENAMETOOLONG`, `-EFAULT`, or `-ERANGE`.

This document will evolve as the GCC milestone lands and new system calls are
added.
