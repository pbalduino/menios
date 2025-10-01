# Road to Shell Readiness

This document tracks the kernel work needed to bring up a practical shell
environment. It highlights the four foundation issues that unlock the next
phase of the userland experience and captures their current status, remaining
work, and relationships.

## #149 – Init as PID 1

**Status:** In progress

- [x] Boot-time launcher (`user_init_launch`) queues a dedicated init process.
- [ ] Minimal init ELF prints a banner so the boot sequence confirms PID 1 is
      running (stdout still needs to be re-bound to `/dev/tty0`).
- [ ] Provide default environment variables (`PATH`, `HOME`), chdir to `/` and
      dup `/dev/tty0` onto the standard streams.
- [x] Replace the temporary idle loop with logic that `execve`s the next stage
      (user demo today, `mosh` tomorrow) and respawns it if it exits.
- [x] Mount the FAT32 root filesystem so `/bin` is available before launching
      the shell payload.

## #146 – Device Filesystem (`/dev`)

**Status:** In progress

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
- [ ] Add regression tests that spawn a child, exec a trivial ELF, and verify
      `waitpid()` semantics.
- [ ] Audit file-descriptor cloning (`CLOEXEC`, controlling terminal) and env
      propagation to support init’s future supervision role.

---

With these items tracked explicitly we can close issues as the remaining boxes
are checked and keep the shell roadmap aligned with kernel progress.
