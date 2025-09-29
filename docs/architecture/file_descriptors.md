# File Descriptor Subsystem

meniOS now exposes a Unix-like file descriptor model to both the kernel and
user processes. Each process owns an independent descriptor table that tracks
opaque `file_t` handles with reference counting and close-on-exec semantics.

## Core Concepts

- **`file_t`** – encapsulates an operations table (`read`, `write`, `close`), a
  refcount, and optional private data. Handles are reference counted so the same
  object can be installed in multiple descriptor tables.
- **Per-process tables** – `proc_file_table_init/clone/cleanup` manage the
  descriptor array embedded in `proc_info_t`. Cloning bumps refcounts while
  exec drops any descriptors flagged with `FD_CLOEXEC`.
- **Kernel bootstrap** – `file_system_init()` now runs after the heap is ready
  and installs a ring-buffer-backed stdin plus two serial-backed streams for
  stdout and stderr. Consoles and logging go through these descriptors rather
  than hard-coded serial writes, while keystrokes arriving from the PS/2 driver
  are pushed into stdin so user processes can `read()` them. A dedicated
  `/dev/input/kbd` character device exposes timestamped key events for
  applications that need structured input rather than raw bytes.
- **Syscall surface** – the dispatcher wires up `read`, `write`, `close`,
  `dup`, `dup2`, `fcntl`, and `pipe`. Kernel helpers validate descriptors and
  translate return codes into errno values for userspace.

## Pipes

`src/kernel/fs/pipe.c` implements an anonymous pipe using a shared
`pipe_shared_t` backing structure:

- **Locking & wake-ups** – a `kmutex` guards the ring buffer while a pair of
  `kcondvar`s coordinate blocking writers/readers.
- **Resource lifetime** – endpoints hold the shared structure via refcounts.
  Closing an endpoint decrements the reader/writer count and tears down the
  shared buffer when both sides are gone.
- **sys_pipe()** – allocates the shared state, installs the two descriptors in
  the caller’s table, and returns the fd pair to userspace.

## Userspace support

`src/libc/unistd.c` exposes thin system-call wrappers (`read`, `write`, `close`,
`dup`, `dup2`, `pipe`) and the existing libc now includes `<sys/fcntl.h>` plus
new `<unistd.h>` prototypes. The user-mode demo exercises pipe creation,
clones stdout with `dup`, and streams data from the parent to the forked child.

## Debugging & Testing

- `make test` covers the descriptor helpers indirectly via the kmutex suite.
- `make run` boots the kernel, starts three user demo processes, fork/exec the
  demo elf, and prints messages proving the pipe round-trip works.

This subsystem unlocks richer IPC primitives and sets the stage for higher
level facilities (sockets, stdio, shell) built atop the same descriptor API.
