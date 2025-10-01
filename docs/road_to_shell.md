# Road to Shell Readiness

This document tracks the kernel work needed to bring up a practical shell
environment. It highlights the four foundation issues that unlock the next
phase of the userland experience and captures their current status, remaining
work, and relationships.

## #149 – Init as PID 1

**Status:** In progress

- [x] Boot-time launcher (`user_init_launch`) queues a dedicated init process.
- [x] Minimal init ELF prints a banner so the boot sequence confirms PID 1 is
      running.
- [ ] Provide default environment variables (`PATH`, `HOME`), chdir to `/` and
      dup `/dev/tty0` onto the standard streams.
- [x] Replace the temporary idle loop with logic that `execve`s the next stage
      (user demo today, `mosh` tomorrow) and respawns it if it exits.
- [x] Mount the FAT32 root filesystem so `/bin` is available before launching
      the shell payload.

## #146 – Device Filesystem (`/dev`)

**Status:** Not started

- [ ] Implement `devfs` as a VFS driver that exposes the registered character
      devices under `/dev` (`/dev/tty0`, `/dev/console`, `/dev/null`, etc.).
- [ ] Ensure `open()`/`stat()` resolve device nodes and honour permissions.
- [ ] Allow init to mount `devfs` early so user processes can rely on the
      `/dev` hierarchy.

## #145 – In-memory `/tmp`

**Status:** Not started

- [ ] Implement a lightweight tmpfs/ramfs VFS backend for `/tmp`.
- [ ] Mount it during boot and ensure correct permissions (`1777`).
- [ ] Verify shell workflows (temporary files, redirections) succeed using the
      new filesystem.

## #93 – Fork/Exec Runtime

**Status:** Available, needs hardening

- [x] Kernel already exposes `fork()` and `execve()` (see `proc_fork` and
      `proc_execve`).
- [ ] Add regression tests that spawn a child, exec a trivial ELF, and verify
      `waitpid()` semantics.
- [ ] Audit file-descriptor cloning (`CLOEXEC`, controlling terminal) and env
      propagation to support init’s future supervision role.

---

With these items tracked explicitly we can close issues as the remaining boxes
are checked and keep the shell roadmap aligned with kernel progress.
