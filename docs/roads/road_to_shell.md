# Road to the mosh Shell on meniOS

🎯 **Goal**: Deliver an interactive POSIX-style shell for meniOS, complete with
process control, pipelines, and user-friendly tooling.

## Current Status

- **Boot & supervision**: PID 1 init supervises children (#149/#150 ✅).
- **Process lifecycle**: wait/waitpid and zombie reparenting in place (#145/#146 ✅).
- **Filesystem/navigation**: VFS read-only path ready; `getcwd(2)`/`chdir(2)` now live (#147 ✅).
- **Pipes & signals**: Anonymous pipes implemented (#102 ✅); signal delivery prototype ready (#103 🟡).
- **Input**: Keyboard events surfaced via `/dev/input/kbd`; mouse pending (#32 ✅, #143/#144 🟡).
- **Terminal I/O**: `/dev/console` now bridges VGA output with serial logging and keyboard input (#138 ✅).

Remaining core pieces: shell REPL & execution pipeline (#161-#165), and
quality-of-life features (history, completion, job control).

## Milestones & Timeline

### Phase 0 – Foundations (COMPLETE)
- #145 wait/waitpid
- #146 zombie handling
- #149 init supervisor
- #150 boot integration
- #147 getcwd/chdir

### Phase 1 – Shell Prerequisites (Week 1 – COMPLETE)
1. **#148 Environment variables (DONE)**
   - `getenv/setenv/unsetenv` syscalls wired; PATH/HOME ready for shell

### Phase 2 – Core Shell (Week 2-3)
2. **#161 REPL & parsing (DONE)** – line reader, tokenizer, command AST
3. **#162 Command execution (DONE)** – PATH lookup, fork/exec workflow
4. **#163 Built-in commands (2-3 days)** – `cd`, `pwd`, `exit`, `env`
5. **#164 Basic redirection (2-3 days)** – `>`, `>>`, `<` via `dup2`
6. **#165 Pipe support (3-4 days)** – pipelines using existing `pipe(2)`
🔹 *Milestone*: Usable shell with pipelines and redirection

### Phase 3 – Quality of Life (Week 4-5)
7. **#156 Command history (3-5 days)**
8. **#157 Tab completion (4-6 days)**
9. **#160 Line editing (4-6 days)** – Emacs/readline-style controls
🔹 *Milestone*: Comfortable interactive shell

### Phase 4 – Advanced Features (Week 6+)
10. **#155 Scripting (1-2 weeks)** – control flow, variables
11. **#158 Job control (1-2 weeks)** – background processes, signals
12. **#159 Advanced redirection (3-5 days)** – heredocs, tee-like features
🔹 *Milestone*: Production-grade shell

## Dependency Map

```
Init/Supervision (DONE) ─┐
Filesystem navigation (#147 - DONE) ─┼──→ #161 (REPL)
Environment variables (#148 - DONE) ──┤       ↓
Pipes (#102), Signals (#103) ────────┴──→ #162 (exec) → #163 (built-ins) → #164 (redir) → #165 (pipes)
                                                   ↓
                                    QoL (#156,#157,#160) → Advanced (#155,#158,#159)
```

## Immediate Focus

1. **#161 REPL** – start the shell core.
2. **#162 Command execution** – tie REPL to process launching.
3. **#163 Built-ins** – wire `cd`, `pwd`, `exit`, `env`.

## Integration with Other Roadmaps

- **Road to Doom**: Shell is a prerequisite for userland tooling before larger
  apps can ship.
- **Road to Multi-user**: Completed shell (#147-#165) is the foundation for
  login sessions and user isolation.
- **Device filesystem (devfs/procfs)** will later surface `/dev/tty*` and
  `/proc/*` nodes for richer shell utilities.

## Success Criteria

- Shell starts automatically from PID 1 or via init scripts.
- Supports path navigation, built-ins, pipelines, and basic redirection.
- Handles signals (Ctrl+C, Ctrl+Z in later phases) gracefully.
- Provides history, completion, and editing for day-to-day usability.

Progress snapshot: 8/12 shell tasks complete (Phase 0 + `getcwd/chdir` +
environment variables + REPL + command execution). Next stop: built-ins,
redirection, and pipelines.
