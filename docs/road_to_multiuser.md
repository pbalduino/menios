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

Capturing answers to the open questions before implementation will reduce churn and keep the multi-user initiative on schedule.
