# Road to a Minimal meniOS Shell

This roadmap captures every dependency for shipping a **minimally functional interactive shell** on meniOS.  The goal is for users to boot the OS, reach a prompt, type commands, execute programs, and see their output without relying on developer shortcuts.

## Definition of "Minimal Shell"

A release that meets this milestone must satisfy all of the following:

- Boot to an init process that supervises `/bin/mosh` (or replacement) and restarts it when it exits.
- Display a readable prompt on the framebuffer console and echo keystrokes with basic line editing.
- Launch binaries from `/bin`, inherit stdio, and report exit status to the user.
- Support shell built-ins for `help`, `exit`, basic history, tab insertion, and command path resolution.
- Provide `/tmp`, `/dev`, and `/bin` mounts so pipelines and file redirections can be implemented later.
- Offer diagnostics (serial log + kernel breadcrumbs) that make shell regressions debuggable.

## Milestone Tracker

| Area | Status | Notes |
| --- | --- | --- |
| Init as PID 1 | ✅ Done | PID 1 binds stdio to `/dev/tty0` and execs `/bin/mosh` |
| Supervision Loop | ✅ Done | `waitpid` fix keeps init blocked until child exits |
| Device Filesystem | ✅ Done | `/dev/tty0`, `/dev/console`, `/dev/null`, `/dev/ptmx` registered |
| Temporary FS | ✅ Done | tmpfs mounted at `/tmp` with read/write support validated via shell redirection |
| Fork/Exec Runtime | ✅ Done | `fork`, `execve`, `waitpid`, descriptor cloning working |
| Shell Prompt UX | ✅ Done | Inline caret, history navigation, prompt redraw regression tests |
| Default Environment | ✅ Done | Seed `PATH`, `HOME`, `PWD`; chdir to `/` before launching shell (#180, #185) |
| Launchable Utilities | ✅ Done | Added `/bin/echo`, `/bin/cat`, `/bin/env`, `/bin/true`, `/bin/false`, `/bin/ls`, `/bin/kill`, `/bin/ps` (#183/#187) |
| Regression Coverage | 🔄 In progress | `test_mosh_line` + `test_mosh_exec` cover caret redraw and waitpid status |

## Kernel Foundations

1. **PID 1 Contract** *(Complete)*
   - `user_init_launch` starts PID 1, binds stdio, prints banner.
   - Init sleeps in `waitpid` until a child exits, then relaunches the shell.
2. **Syscall Surface** *(Complete)*
   - `fork`, `execve`, `waitpid`, `read`, `write`, `dup2`, `close`, `sleep` reachable from userland.
   - Future: `pipe`, `signal`, `mmap` enhancements for scripting.
3. **Filesystem Mounts** *(Complete / Follow-up)*
   - FAT32 root mounted before init launches userland.
   - `/dev` (devfs) mounted before the shell reads stdin/stdout.
   - `/tmp` mounted (verify permissions + usage from shell scripts).
4. **Console and Input Drivers** *(Complete)*
   - Framebuffer console receives kernel + user output.
   - PS/2 keyboard driver feeds canonical stdin buffer for mosh.
5. **Diagnostics** *(Complete)*
   - Serial logging stays available even when framebuffer prompt is active.
   - Kernel breadcrumbs retained around scheduling and waitpid.

## Shell Feature Checklist

- [x] Prompt renders cleanly after every command (no interleaving with kernel logs).
- [x] Caret overlay follows cursor; hide/show around redraws.
- [x] History navigation (↑/↓) restores previous commands; scratch buffer preserved.
- [x] Left/right cursor movement, home/end, backspace/delete editing.
- [x] Tab completion cycles filesystem entries; `cd` filters to directories only.
- [x] `help` prints quick usage; `exit` terminates shell.
- [x] External commands auto-prefix with `/bin/` when no slash present.
- [x] Forked child inherits stdio, reports failure via stderr.
- [x] Shell waits for child completion and reports non-zero exit status.
- [x] Basic pipelines and `<`/`>` redirection supported for foreground commands (#186).
- [x] `Ctrl+C` aborts foreground commands and pipelines via `SYS_PROC_KILL`.
- [x] `PATH` search order configurable once environment support lands (#185).
- [x] Tests ensure waitpid returns correct PID and shell stays in supervision loop (#182).

## Blocking TODOs

1. **Environment Seeding** (#180, #185)
   - ✅ `src/usermode/init.c` seeds `PATH=/bin`, `HOME=/`, `PWD=/` before launching mosh; fallback env in mosh mirrors the same defaults.
   - ✅ `test/test_mosh_line.c` asserts the default environment is visible to the shell.
   - ✅ PATH search order configuration implemented (#185).
2. **tmpfs Validation** (#181)
   - ✅ Host-side regression (`test/test_tmpfs.c`) mounts tmpfs and verifies create/read/write through the VFS layer.
3. **Waitpid Regression Test** (#182)
   - ✅ Host-side coverage (`test/test_init_supervision.c`) exercises the kernel waitpid handler for both blocking and `WNOHANG` paths, verifying PID 1 remains in the waiting state until the child exits.
4. **Line Editor Coverage** (#184)
   - ✅ Extended `test_mosh_line` to cover delete, history navigation, and newline flows.

## Verification Matrix

| Scenario | Owner | Status |
| --- | --- | --- |
| Boot to prompt via `make run` | manu/ci | ✅ Stable |
| Typing commands echoes characters | manu | ✅ Stable |
| Backspace redraw | `test/test_mosh_line.c` | ✅ Covered |
| History navigation | manu | 🔄 Manual only |
| Launch external binary (`/bin/user_demo`) | manu | ✅ Manual |
| Shell exit + respawn | manu | ✅ Manual |
| tmpfs workflow (`cat > /tmp/foo`) | test/test_tmpfs.c | ✅ Covered |

## Future Enhancements (Post-Minimal Shell)

### IPC and Process Control
- Expand pipelines to support advanced syntax (append, stderr redirection) once IPC primitives land (#209). Kernel pipe infrastructure (#206), the `pipe()` syscall/user wrapper (#207), shell integration (#208), and basic `|`, `<`, `>` handling (#165/#164) are already in place.
- Signal delivery/handling for user processes (Ctrl+C integrated, advanced policies pending) (#213 done, #214 todo).
- Job control (background tasks, `wait` builtin) (#158).
- Process management utilities (`ps`, basic `kill` implemented; richer semantics future) (#187 ✅).

### Shell Usability Features
- ✅ Working directory management (`getcwd`, `chdir` syscalls) (#147) - **NOW COMPLETE!**
- ✅ Tab completion for files and directories (#197).
- ✅ Current directory in prompt (#222) - Dynamic prompt shows cwd in shell banner.
- ✅ Emacs-style line editing (Ctrl+A, Ctrl+E) (#198).
- ✅ Reverse command search (Ctrl+R) (#199).
- ✅ Clear screen shortcut (Ctrl+L) (#200).
- ✅ Command history with up/down arrows (#156).

### System Integration
- Login/getty integration for multiple TTYs (#175-#178).

Keep this document updated whenever shell-related PRs merge or new blockers appear.  Treat the milestone as complete only when every "TODO" above is resolved and automated coverage eliminates the regressions that inspired this roadmap.
