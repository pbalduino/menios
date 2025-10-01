# Road to Shell Readiness

This document tracks the kernel work needed to bring up a practical shell
environment. It highlights the four foundation issues that unlock the next
phase of the userland experience and captures their current status, remaining
work, and relationships.

## #149 – Init as PID 1

**Status:** In progress

- [x] Boot-time launcher (`user_init_launch`) queues a dedicated init process.
- [x] Minimal init ELF prints a banner so the boot sequence confirms PID 1 is
      running (stdio now targets `/dev/tty0`).
- [x] Bind PID 1 standard streams to `/dev/tty0` so init and userland output is
      visible on the console.
- [ ] Provide default environment variables (`PATH`, `HOME`) and chdir to `/`
      before launching userland.
- [x] Replace the temporary idle loop with logic that `execve`s the next stage
      (boots `/bin/mosh` with a `/bin/user_demo` fallback while supervision work
      continues).
- [x] Mount the FAT32 root filesystem so `/bin` is available before launching
      the shell payload.

## #146 – Device Filesystem (`/dev`)

**Status:** Complete

- [x] Implement `devfs` as a VFS driver that exposes the registered character
      devices under `/dev` (`/dev/tty0`, `/dev/console`, `/dev/null`, etc.).
- [x] Ensure `open()` resolves device nodes and honours basic read/write
      capabilities.
- [x] Mount `devfs` during early `file_system_init` so user processes can open
      `/dev` paths without kernel assistance.

## #145 – In-memory `/tmp`

**Status:** In progress

- [x] Implement a lightweight tmpfs/ramfs VFS backend for `/tmp`.
- [x] Mount it during boot and ensure correct permissions (`1777`).
- [ ] Verify shell workflows (temporary files, redirections) succeed using the
      new filesystem.

## #93 – Fork/Exec Runtime

**Status:** Available, needs hardening

- [x] Kernel exposes `fork()`/`execve()` (see `proc_fork`, `proc_exec_image`,
      and the filesystem-backed syscall path).
- [x] `proc_exec_image()` seeds `argc`/`argv`/`envp` on the new user stack so
      freshly executed programs observe the expected SysV ABI entry contract.
- [ ] Add regression tests that spawn a child, exec a trivial ELF, and verify
      `waitpid()` semantics.
- [ ] Audit file-descriptor cloning (`CLOEXEC`, controlling terminal) and env
      propagation to support init’s future supervision role.

## User-shell Milestone

- [x] `/bin/mosh` now ships as a freestanding ELF that prints a prompt, accepts
      user input on stdin, and handles simple built-ins (`help`, `exit`).
- [x] PID 1 now routes stdio to `/dev/tty0`, so the init banner and mosh prompt
      land on the visible console.
- [x] mosh resolves commands (defaulting to `/bin/<name>`), forks, and calls
      `execve()` using the new argument/environment plumbing.

---

With these items tracked explicitly we can close issues as the remaining boxes
are checked and keep the shell roadmap aligned with kernel progress.
