# Road to Multi-User Support

This document outlines the work needed to evolve Menios from a single-user prototype into a kernel and userland that supports proper user identities, permissions, and session management. Each stage lists prerequisites, deliverables, and risks so we can plan incremental milestones without destabilising the rest of the system.

## Goals & Non-Goals

- Provide per-process credentials (uid, gid, effective ids) that inform kernel permission checks.
- Track ownership and mode bits across every filesystem the kernel exposes.
- Gate privileged syscalls and device access behind access-control decisions.
- Deliver a login/authentication flow that spawns isolated shells or apps for a given user.
- Update libc and core utilities to respect and surface permissions.
- Ship tests and documentation that cover the new behaviour.

Out of scope for the first iteration: discretionary ACLs, SELinux-style MAC, network directory services, or PAM-like plugin stacks. We also defer password ageing, multiple authentication factors, and audited logging until the base feature set is stable.

## Current Starting Point

- Kernel processes likely inherit a single implicit user context (effective root).
- Filesystems (initrd, ramfs, ext-like) do not store or enforce ownership metadata.
- Syscall table lacks UID/GID or permission-oriented calls.
- Userland shell and apps assume unrestricted access.
- Tests do not exercise permission boundaries.

## Phase Breakdown

### Phase 1: Credential Infrastructure

- Extend task/process descriptors (e.g., `struct task`) with `uid`, `gid`, `euid`, `egid`, and `umask`.
- Update scheduler, fork/exec, and signal paths to carry credentials forward.
- Introduce kernel helpers to compare caller credentials and privilege level.
- Wire in bootstrap defaults: PID 0/1 run as `root` (uid 0) with defined `system` group.

**Deliverables**
- Updated kernel data structures and accessors.
- Credentials preserved across lifecycle syscalls.
- Documentation for new struct fields (`include/` headers, developer docs).

**Risks / Open Questions**
- Impact on task struct layout and ABI; ensure alignment on both x86_64 and i386 builds.
- Decide whether kernel threads always run as root or inherit from parent.

### Phase 2: Syscall Surface & Enforcement

- Add `getuid`, `geteuid`, `getgid`, `getegid`, `setuid`, `setgid`, `setreuid`, `setregid`, `umask`.
- Implement permission checks within existing syscalls: `open`, `truncate`, `unlink`, `chmod`, `chown`, `kill`, `signal`, device IO.
- Define error codes (`-EPERM`, `-EACCES`, `-ENOTTY` for restricted TTY operations).
- Update syscall dispatch to reject privileged operations when caller lacks rights.

**Deliverables**
- Kernel handlers for new syscalls.
- libc wrappers and headers in `user/include/`.
- Regression matrix covering success/failure combinations.

**Risks / Open Questions**
- Need consistent privilege escalation rules (setuid binaries, kernel modules).
- Determine minimum viable subset if full POSIX coverage is too large for first release.

### Phase 3: Filesystem Metadata & Tooling

- Extend VFS inode representation with owner UID, GID, and POSIX mode bits.
- Update on-disk formats (initrd builder, ramfs, ext2 if present) to store metadata.
- Modify `open`, `stat`, directory iteration, and file creation to respect default umask and permissions.
- Update build tooling (`make userland`) to embed ownership metadata when packaging binaries.

**Deliverables**
- Revised VFS and drivers with permission-aware operations.
- Migration tools/scripts for existing filesystem images.
- Updated docs detailing filesystem format changes.

**Risks / Open Questions**
- Backward compatibility with existing disk images; may need conversion step.
- Ensuring boot-critical binaries remain readable/executable for root-only startup sequence.

### Phase 4: Authentication & Session Management

- Introduce `/etc/passwd`-style credential store (hashed passwords, shell path, uid/gid).
- Implement password hashing (likely SHA-256 or bcrypt-lite) and secure comparison.
- Create `login` program that authenticates and spawns user shell with correct credentials via new `setuid` semantics.
- Update init to launch a login manager on the system console (and optionally serial).

**Deliverables**
- Credential file format and parser in userland.
- `login` utility and supporting libc helpers.
- Adjusted init sequence to require login for interactive sessions.

**Risks / Open Questions**
- Secure storage for password hashes on read-only initrd (may require writable filesystem or overlay).
- Handling headless/embedded workflows where auto-login is desired.

### Phase 5: Userland & Tooling Updates

- Update core apps (`ls`, `cat`, `cp`, shell) to display/heed ownership and mode information.
- Add utilities: `id`, `whoami`, `su`, `passwd` (if feasible).
- Ensure shell builtins respect permission failures and propagate exit codes.
- Provide admin scripts or documentation to add new users/groups offline.

**Deliverables**
- Enhanced user utilities with permission awareness.
- Support scripts for managing `/etc/passwd` and filesystem ownership.
- Manpages or README updates describing new commands and workflow.

**Risks / Open Questions**
- Complexity of `su`/`sudo` semantics; might postpone to later milestone.
- Coordination with existing build/test infrastructure when new binaries appear.

### Phase 6: Hardening & Observability

- Evaluate kernel privilege boundaries: restrict direct hardware IO, device nodes, and syscalls to root.
- Add auditing hooks or logs for failed authentication attempts and permission denials (at least debug builds).
- Consider per-user resource limits (file descriptors, processes) for stability.
- Review attack surface of new syscalls and parsers.

**Deliverables**
- Hardened permission matrix and documented policy.
- Debug logging or tracing for enforcement decisions.
- Optional rate limiting for authentication attempts.

**Risks / Open Questions**
- Logging in kernel needs buffer management; ensure it does not destabilise low-memory scenarios.
- Resource limits may require scheduler/memory manager enhancements.

## Testing Strategy

- Expand Unity host tests in `test/` to cover credential propagation, syscall enforcement, filesystem permission cases, and login parsing.
- Add kernel-side integration smoke tests executed via QEMU (scriptable `make run` scenarios).
- Create golden disk images or initrd snapshots with sample users to validate migration steps.
- Integrate `make test` and `make check` gating into CI to catch permission regressions early.

## Documentation & Developer Enablement

- Update `docs/` with how-to guides on adding users, managing permissions, and debugging credential issues.
- Refresh `MILESTONES.md` with the phased plan and target releases.
- Record code structure changes in architecture notes (`docs/architecture/`).
- Ensure `tasks.json` maps issues to the phases described here.

## Suggested Milestone Ordering

1. Credential infrastructure + syscall scaffolding (Phases 1-2).
2. Filesystem metadata and toolchain support (Phase 3).
3. Authentication path and userland adjustments (Phases 4-5).
4. Hardening, observability, and stretch goals (Phase 6).

Each milestone should land with dedicated tests, doc updates, and a stabilization window before stacking the next phase.

## Open Decisions

- Password hashing algorithm choice and availability within current libc.
- Whether to support multiple concurrent consoles/sessions in the first release.
- Strategy for migrating legacy disk images (scripted conversion vs. rebuild).
- How to expose administrative tasks (shell commands vs. config files vs. GUI later).

## Next Immediate Steps

1. Audit `src/` and `include/` to document current task structures and syscall table.
2. Draft RFC detailing credential struct changes and enforcement policies; review with kernel maintainers.
3. Spike a prototype branch adding dummy UID fields to quantify impact on context switch/scheduler.
4. Inventory filesystem formats and builder scripts to scope metadata migration effort.
5. ✅ **Complete FAT32 metadata work** (#367) - **DONE!**
6. 🚀 **Add chmod support to FAT32** (#365) - READY TO START
7. 🚀 **Add utime support to FAT32** (#317) - READY TO START
8. **Add permission enforcement** to existing syscalls that already have metadata infrastructure

Capturing answers to the open questions before implementation will reduce churn and keep the multi-user initiative on schedule.

## Related Issues & Current Work

### Completed Foundation
- ✅ **#65** - VFS layer (provides permission infrastructure)
- ✅ **#189** - FAT32 write support (persistent storage available)
- ✅ **#181** - tmpfs validation (in-memory filesystem working)
- ✅ **#364** - Stubbed libc functions (stat family complete)
- ✅ **#367** - Parse rich FAT32 metadata (**COMPLETE!** - enables chmod/utime on FAT32)

### Active Metadata Work (Building Blocks for Multi-User)
- 🚀 **#365** - chmod/fchmod syscalls (tmpfs complete, **FAT32 READY TO START**)
  - Unblocked by #367 completion
  - Map POSIX permissions to/from DOS read-only bit
  - Timeline: 3-5 days
- 🚀 **#317** - utime syscall (tmpfs complete, **FAT32 READY TO START**)
  - Unblocked by #367 completion
  - Convert POSIX timestamps to/from DOS date/time
  - Timeline: 3-5 days
- ⏳ **#366** - Pseudo-fs metadata support (devfs/procfs consistency)
  - Blocked by #365 and #317
  - Timeline: 3-5 days after blockers

### Planned Multi-User Work
- 📋 **#232** - Implement chmod and chown syscalls (comprehensive plan exists)
  - Builds on #365 (chmod infrastructure)
  - Adds chown/fchown/lchown for ownership changes
  - Includes permission validation and utilities

### Future Requirements
- **Phase 1:** Task struct extensions (uid, gid, euid, egid fields)
- **Phase 2:** Syscall surface (getuid, setuid, setgid, umask, etc.)
- **Phase 3:** Filesystem integration (ownership stored on-disk)
- **Phase 4:** Authentication (/etc/passwd, login program)
- **Phase 5:** Userland utilities (id, whoami, su, passwd)
- **Phase 6:** Hardening and auditing

### Key Dependencies
**Before starting Phase 1:**
1. ✅ Complete #367 (FAT32 metadata foundation) - **DONE!**
2. 🚀 Complete #365, #317 (chmod/utime on all filesystems) - **READY TO START**
3. ⏳ Complete #366 (pseudo-fs consistency) - blocked by #365, #317

**These provide the filesystem metadata layer that multi-user support will build upon.**

---

**Last Updated:** 2025-10-28
**Status:** ✅ #367 complete! #365 and #317 ready to start.
**Timeline:** 2-3 weeks for full metadata support, then 6-12 months for full multi-user support
