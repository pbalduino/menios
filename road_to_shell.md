# Road to the mosh Shell on meniOS

🎯 **Goal**: Deliver an interactive POSIX-style shell for meniOS, complete with
process control, pipelines, and user-friendly tooling.

## Current Status

- **Boot & supervision**: PID 1 init supervises children (#153/#154 ✅).
- **Process lifecycle**: wait/waitpid and zombie reparenting in place (#149/#150 ✅).
- **Filesystem/navigation**: VFS read-only path ready; `getcwd(2)`/`chdir(2)` now live (#151 ✅).
- **Pipes & signals**: Anonymous pipes implemented (#102 ✅); signal delivery prototype ready (#103 🟡).
- **Input**: Keyboard events surfaced via `/dev/input/kbd`; mouse pending (#32 ✅, #143/#144 🟡).

Remaining core pieces: environment variables (#152), shell REPL & execution
pipeline (#161-#165), and quality-of-life features (history, completion, job
control).

## Milestones & Timeline

### Phase 0 – Foundations (COMPLETE)
- #149 wait/waitpid
- #150 zombie handling
- #153 init supervisor
- #154 boot integration
- #151 getcwd/chdir

### Phase 1 – Shell Prerequisites (Week 1)
1. **#152 Environment variables (2-3 days)**
   - Implement `getenv/setenv/unsetenv`
   - Seed PATH/HOME when init execs the shell

### Phase 2 – Core Shell (Week 2-3)
2. **#161 REPL & parsing (2-3 days)** – line reader, tokenizer, command AST
3. **#162 Command execution (2-3 days)** – PATH lookup, fork/exec workflow
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
Filesystem navigation (#151 - DONE) ─┼──→ #161 (REPL)
Environment variables (#152) ────────┤       ↓
Pipes (#102), Signals (#103) ────────┴──→ #162 (exec) → #163 (built-ins) → #164 (redir) → #165 (pipes)
                                                   ↓
                                    QoL (#156,#157,#160) → Advanced (#155,#158,#159)
```

## Immediate Focus

1. **#152 Environment variables** – unblock command execution by providing
   PATH/HOME/PS1.
2. **#161 REPL** – start the shell core.
3. **#162 Command execution** – tie REPL to process launching.

## Integration with Other Roadmaps

- **Road to Doom**: Shell is a prerequisite for userland tooling before larger
  apps can ship.
- **Road to Multi-user**: Completed shell (#152-#165) is the foundation for
  login sessions and user isolation.
- **Device filesystem (devfs/procfs)** will later surface `/dev/tty*` and
  `/proc/*` nodes for richer shell utilities.

## Success Criteria

- Shell starts automatically from PID 1 or via init scripts.
- Supports path navigation, built-ins, pipelines, and basic redirection.
- Handles signals (Ctrl+C, Ctrl+Z in later phases) gracefully.
- Provides history, completion, and editing for day-to-day usability.

Progress snapshot: 5/12 shell tasks complete (Phase 0 + `getcwd/chdir`). Next
stop: environment variable support to kick off the REPL work.
