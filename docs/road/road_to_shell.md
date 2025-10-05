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
| Temporary FS | 🔄 In progress | tmpfs mounted at `/tmp`; redirection workflows still unverified |
| Fork/Exec Runtime | ✅ Done | `fork`, `execve`, `waitpid`, descriptor cloning working |
| Shell Prompt UX | ✅ Done | Inline caret, history navigation, prompt redraw regression tests |
| Default Environment | ⛳ TODO | Seed `PATH`, `HOME`, `PWD`; chdir to `/` before launching shell (#180) |
| Launchable Utilities | 🔄 In progress | `/bin/mosh` works; expand with diagnostics tools (`/bin/echo`, etc.) (#183) |
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
- [x] Tab inserts literal `\t` (no completion yet).
- [x] `help` prints quick usage; `exit` terminates shell.
- [x] External commands auto-prefix with `/bin/` when no slash present.
- [x] Forked child inherits stdio, reports failure via stderr.
- [x] Shell waits for child completion and reports non-zero exit status.
- [ ] `PATH` search order configurable once environment support lands (#185).
- [ ] Basic pipeline placeholders (`|`, `>`, `<`) recognized (parsing TBD) (#186).
- [ ] Tests ensure waitpid returns correct PID and shell stays in supervision loop (#182).

## Blocking TODOs

1. **Environment Seeding** (#180)
   - Export `PATH=/bin`, `HOME=/`, `PWD=/` in init before execing the shell.
   - Add a regression test that inspects `environ` from `/bin/env` or similar.
2. **tmpfs Validation** (#181)
   - Run manual tests creating files in `/tmp` from the shell and reading them back.
   - Add a host-side unit/integration test that mounts tmpfs and confirms permissions.
3. **Waitpid Regression Test** (#182)
   - Extend coverage beyond unit stubs to integration tests that fork/exec a dummy program and confirm init supervision stays asleep until completion.
4. **Utility Set** (#183)
   - Provide simple `/bin` helpers (`echo`, `cat`, `env`, `true`, `false`) to let shell demos proceed without custom binaries.
5. **Line Editor Coverage** (#184)
   - Extend `test_mosh_line` to cover delete, history navigation, and newline flows.

## Verification Matrix

| Scenario | Owner | Status |
| --- | --- | --- |
| Boot to prompt via `make run` | manu/ci | ✅ Stable |
| Typing commands echoes characters | manu | ✅ Stable |
| Backspace redraw | `test/test_mosh_line.c` | ✅ Covered |
| History navigation | manu | 🔄 Manual only |
| Launch external binary (`/bin/user_demo`) | manu | ✅ Manual |
| Shell exit + respawn | manu | ✅ Manual |
| tmpfs workflow (`cat > /tmp/foo`) | manu | ⛳ TODO |

## Future Enhancements (Post-Minimal Shell)

- Command pipelines (`pipe`, `dup2`) once IPC primitives land (#165, #102).
- Signal handling to interrupt running commands (CTRL+C) (#103).
- Job control (background tasks, `wait` builtin) (#158).
- Tab completion and richer readline behaviour (#157).
- Login/getty integration for multiple TTYs (#175-#178).
- Process management utilities (`ps`, `kill`) (#187).

Keep this document updated whenever shell-related PRs merge or new blockers appear.  Treat the milestone as complete only when every "TODO" above is resolved and automated coverage eliminates the regressions that inspired this roadmap.
