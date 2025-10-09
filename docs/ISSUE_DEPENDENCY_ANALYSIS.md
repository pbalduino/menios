# meniOS Issue Dependency Analysis

This document provides a comprehensive analysis of dependencies between open issues in the meniOS project, helping prioritize development efforts.

**Last Updated**: 2025-10-13
**Total Issues**: 44 new issues created recently
- First batch: #180-#204 (25 issues)
- I/O scheduler: #205 (1 issue)
- IPC breakdown: #206-#219 (14 issues from breaking #102, #103, #104)
- ioctl: #220 (1 issue)
- Userland driver research: #223 (1 issue)
- Additional utility issues: #204 (closed; 3 issues total in recent work)

## 🎯 Critical Path Issues

These issues form the backbone of the system and should be prioritized:

### Tier 1: Foundation ✅ (COMPLETE!)
1. ✅ **#35** - kmalloc implementation (CLOSED - enables all kernel features)
2. ✅ **#57** - VM manager (CLOSED - vm_map/vm_unmap/vm_clone)
3. ✅ **#96** - File descriptor management (CLOSED)
4. ✅ **#34** - Preemptive scheduler (CLOSED)
5. ✅ **#60** - Filesystem syscalls (CLOSED)
6. ✅ **#65** - VFS layer (CLOSED)

### Tier 2: Toolchain (CRITICAL PATH - Major Progress!)
7. ✅ **#192** - crt0 runtime startup code (CLOSED)
8. ✅ **#193** - Minimal userland libc (CLOSED)
9. **#194** - Syscall ABI documentation
10. **#195** - Userland build system
11. **#29** - Cross-compiler toolchain integration

### Tier 3: Shell Milestone (Complete! 🎉)
12. ✅ **#148** - Environment variables support (CLOSED)
13. ✅ **#180** - Environment seeding in init (CLOSED)
14. ✅ **#181** - tmpfs validation (CLOSED)
15. ✅ **#182** - waitpid regression test (CLOSED)
16. ✅ **#183** - /bin utilities (echo, cat, env, true, false) (CLOSED)
17. ✅ **#184** - Line editor coverage (CLOSED)
18. ✅ **#185** - PATH search configuration (CLOSED)
19. ✅ **#186** - Pipeline placeholders (CLOSED)

### Tier 4: Threading Support (1/6 Complete! Foundation Ready! 🎉)
20. ✅ **#108** - Kernel threading infrastructure (CLOSED)
21. **#109** - pthread API and POSIX threading support
22. **#110** - Thread-safe C library (libc)
23. **#111** - Advanced pthread synchronization primitives
24. **#112** - Thread debugging and profiling support
25. **#113** - Thread-aware system calls and kernel integration

## 📊 New Issue Categories

### 🛠️ Toolchain & Compilation (Issues #29, #192-#196)
```
#192 (crt0) ──┬──→ #193 (libc) ──┬──→ #195 (build system)
              │                   │
#194 (ABI docs)                  └──→ #29 (toolchain complete)
                                       │
              ┌────────────────────────┴────────────────────┐
              ↓                                             ↓
         #190 (TCC port)                            #191 (binutils port)

#196 (Fish shell research) - future exploration
```

**Dependencies**:
- #192: No dependencies (start now!)
- #193: Depends on #192, #148
- #194: No dependencies (start now!)
- #195: Depends on #192, #193
- #29: Depends on #192, #193, #194, #195
- #190: Depends on #29, #193, #102, #189
- #191: Depends on #29, #193, #189

**Priority**: **CRITICAL** - Enables all userland development

### 🐚 Shell Milestone (Issues #180-#188)
```
#148 (env vars) ──┬──→ #180 (env seeding)
                  ├──→ #185 (PATH search)
                  └──→ #188 (env utility)

#151 (tmpfs) ──────→ #181 (tmpfs validation)

#145 (waitpid) ────┐
                   ├──→ #182 (waitpid tests)
#149 (init) ───────┘

#183 (/bin utilities) - no dependencies

#184 (line editor coverage) - no dependencies

#186 (pipeline placeholders) - no dependencies

#103 (signals) ────→ #187 (ps/kill)
```

**Status**: Most shell features already implemented! These are polish and testing.

### 🎨 Shell UX Features (Issues #197-#201, #222)
```
#148 (env vars) ──┐
                  ├──→ #197 (tab completion) ✅
#147 (getcwd) ────┼──→ #222 (current dir in prompt) ✅
                  └──→ #197 (tab completion) ✅

#156 (history) ✅ ───→ #199 (Ctrl+R search) ✅

#198 (Ctrl+A/E) ✅
#200 (Ctrl+L) ✅

#143 (PS/2 mouse) ──┐
                    ├──→ #201 (mouse selection)
#144 (USB mouse) ───┘
```

**Priority**: Medium - Quality of life improvements (env utility complete ✅)

### 💾 File System & I/O (Issues #189, #205)
```
#60 (file I/O) ────→ #189 (FAT32 write support)

#114 (block device) ──┐
#62 (AHCI driver)  ───┼──→ #205 (Elevator I/O scheduler) ✅ COMPLETE
#63 (block cache)  ───┘
```

**Status**:
- #189: ⛳ TODO - High priority (needed for save files, native compilation output)
- #205: ✅ COMPLETE - Elevator I/O scheduler now improves disk performance!

### 🔌 IPC: Pipes & FIFOs (Issues #206-#209, from #102) ✅ **COMPLETE!**
```
#96 (fd mgmt) ─────┐
                   ├──→ ✅ #206 (Pipe data structure) - COMPLETE
#40 (condvar) ─────┘       │
                           ↓
                    ✅ #207 (Pipe syscall API) - COMPLETE
                           │
                           ↓
                    ✅ #208 (Shell pipelines) - COMPLETE ──→ #203 (bug fix)
                           │
                           ↓
#65 (VFS) ────────→ ✅ #209 (Named FIFOs) - COMPLETE
```

**Sequential implementation**: ✅ #206 → ✅ #207 → ✅ #208 → ✅ #209

**Priority**: ✅ **COMPLETE** - All pipe and FIFO functionality implemented!

### 🚦 IPC: Signals (Issues #210-#214, from #103) (3/5 Complete!)
```
#93 (fork/exec) ───┐
                   ├──→ ✅ #210 (Signal bookkeeping) - COMPLETE
#34 (scheduler) ───┘       │
                           ↓
                    ✅ #211 (Signal syscalls) - COMPLETE
                           │
                           ↓
                    ✅ #212 (Signal delivery) - COMPLETE
                           │
                           ↓
                    #213 (Shell Ctrl+C) ──→ #187 (ps/kill) ✅
                           │
                           ↓
#109 (pthread) ────→ #214 (Advanced signals - optional)
```

**Sequential implementation**: ✅ #210 → ✅ #211 → ✅ #212 → ✅ #213 → #214

**Priority**: High - Essential for process control (delivery path working!)

### 🧠 IPC: Shared Memory (Issues #215-#219, from #104) ✅ **COMPLETE!**
```
#57 (VM mgr) ──────┐
                   ├──→ ✅ #215 (Shared mem manager) - COMPLETE
#89 (mmap) ────────┘       │
                           ↓
                    ✅ #216 (Shared mem syscalls) - COMPLETE
                           │
                           ↓
                    ✅ #217 (Reference counting) - COMPLETE
                           │
                           ↓
                    ✅ #218 (Test suite) - COMPLETE
                           │
                           ↓
                    ✅ #219 (Documentation) - COMPLETE
```

**Sequential implementation**: ✅ #215 → ✅ #216 → ✅ #217 → ✅ #218 → ✅ #219

**Status**: ✅ **COMPLETE!** All shared memory infrastructure is implemented and tested!

**Priority**: Medium - Useful for high-performance IPC

### 🎛️ Device Control (Issue #220) ✅ **COMPLETE**
```
#96 (fd mgmt) ─────┐
                   ├──→ ✅ #220 (ioctl syscall) - COMPLETE
#60 (file syscalls) ──┘
```

**Priority**: Medium - Useful for terminal and device control

### 🐛 Bug Fixes (Backlog: #202)
```
#137 (/dev/zero) ──→ #202 (EOF bug)

#208 (shell pipelines) ──┐
                         ├──→ ✅ #203 (pipeline hang bug)
#165 (pipeline) ─────────┘
```

✅ #203 closed after fixing mosh's environment updates (pipelines like `echo hi | cat` now succeed).

**Priority**: Deferred - Track #202 once higher-priority work lands

## 🏗️ Updated Implementation Phases

### Phase 1: Core Foundation ✅ (COMPLETE!)
**Goal**: Basic kernel functionality
- ✅ #35: kmalloc implementation (CLOSED)
- ✅ #57: VM manager (vm_map/vm_unmap) (CLOSED)
- ✅ #34: Preemptive scheduler (CLOSED)
- ✅ #36: mutex implementation (CLOSED)
- ✅ #96: File descriptor management (CLOSED)
- ✅ #60: Filesystem syscalls (CLOSED)
- ✅ #65: VFS layer (CLOSED)

**Status**: Foundation is solid! 🎉

### Phase 2: Toolchain (HIGHEST PRIORITY!)
**Goal**: Enable standard C development
- **#192**: crt0 runtime (1 week)
- **#194**: Syscall ABI docs (1 week, parallel)
- **#193**: Minimal libc (3-4 weeks)
- **#195**: Userland build system (1 week)
- **#29**: Toolchain integration (1-2 weeks)

**Timeline**: 2-3 months
**Why Critical**: Enables all other userland development

### Phase 3: Shell Milestone
**Goal**: Complete interactive shell
- ✅ #148: Environment variables
- ✅ #180: Environment seeding
- ✅ #181: tmpfs validation
- ✅ #182: waitpid tests
- ✅ #183: /bin utilities
- ✅ #184: Line editor tests
- ✅ #185: PATH search
- ✅ #186: Pipeline placeholders
- ✅ #187: ps/kill utilities
- ✅ #188: env utility

**Timeline**: 1-2 months (many already done!)
**Status**: Complete! Remaining backlog bug #202 sits outside the milestone

### Phase 4: Threading Support
**Goal**: Full multithreading capability
- **#108**: Kernel threading infrastructure
- **#109**: pthread API and POSIX threading
- **#113**: Thread-aware system calls
- **#110**: Thread-safe C library
- **#111**: Advanced pthread synchronization
- **#112**: Thread debugging and profiling

**Timeline**: 3-4 months
**Why Important**: Enables modern multithreaded applications

### Phase 5: IPC & Advanced Features
**Goal**: Inter-process communication and enhanced functionality

**IPC Track A - Pipes** (High Priority):
- **#206**: Pipe data structure (1-2 weeks)
- **#207**: Pipe syscall API (1 week)
- **#208**: Shell pipelines (1-2 weeks)
- **#209**: Named FIFOs - optional (2 weeks)

**IPC Track B - Signals** (High Priority):
- **#210**: Signal bookkeeping (1-2 weeks)
- **#211**: Signal syscalls (1-2 weeks)
- **#212**: Signal delivery (2-3 weeks)
- **#213**: Shell Ctrl+C ✅
- **#214**: Advanced signals - optional (3-4 weeks)

**IPC Track C - Shared Memory** (✅ COMPLETE!):
- ✅ **#215**: Shared mem manager - COMPLETE
- ✅ **#216**: Shared mem syscalls - COMPLETE
- ✅ **#217**: Reference counting - COMPLETE
- ✅ **#218**: Test suite - COMPLETE
- ✅ **#219**: Documentation - COMPLETE

**File System & Performance**:
- **#189**: FAT32 write support
- **#205**: Elevator I/O scheduler ✅ COMPLETE

**Device Control**:
- ✅ **#220**: ioctl syscall - COMPLETE

**Shell UX**: #197-#200 (tab completion, Ctrl shortcuts) — #197 ✅, #198 ✅, #199 ✅, #200 ✅, #222 (cwd prompt) ✅

**Timeline**: 3-5 months (tracks can run in parallel)

### Phase 6: Native Compilation (Long Term)
**Goal**: Compile on meniOS itself
- **#190**: TCC port
- **#191**: binutils port
- Possibly GCC port (future)

**Timeline**: 6-12 months
**Status**: Research phase (#196 Fish shell)

## 🔴 Current Blocking Relationships

### ✅ Ready to Start NOW (No Dependencies):
- **#194 (ABI docs)** - Start immediately! (Critical path)
- **#213 (Shell Ctrl+C)** - Completed (#210 ✅, #211 ✅, #212 ✅)
- **#218 (Shared mem tests)** - Start immediately! (#217 ✅ complete!)

### ⏳ Blocked, Waiting On:
- **#29 (toolchain)** blocks on: #194, #195
- **#197 (tab completion)** ✅ (dependency #147 satisfied)
- **#199 (Ctrl+R)** ✅ (dependency #156 satisfied)
- **#201 (mouse selection)** blocks on: #143 or #144 (mouse drivers)
- **#187 (ps/kill)** ✅ completed (#213 complete)

### 🔗 Parallel Development Opportunities:
1. **Toolchain** (#192-#195) - Critical path
2. **Shell polish** (#183, #184, #186) - Parallel (Ctrl+A/E, Ctrl+L, history ✅)
3. **Threading** (#108-#113) - Parallel after foundation
4. **File system** (#189) - Parallel
5. **Bug fixes** (#202 backlog) - Track separately

## 🎯 Recommended Focus Areas

### 🚀 **Immediate Next Steps (This Week!)**

**Critical Path (Start Now)**:
1. **#192** - Implement crt0 runtime (assembly, 1 week)
2. **#194** - Document syscall ABI (documentation, 1 week)

**Parallel Development**:
3. **#183** - Implement /bin utilities (C, 1-2 weeks)
4. **#184** - Extend line editor tests (C, 1 week)

### 📅 **Next 2-4 Weeks**
1. **#148** - Environment variables (prerequisite for many)
2. **#193** - Minimal libc (after #192, #148 done)
3. **#186** - Pipeline placeholders (parser work)
4. **#189** - FAT32 write support (file system)

### 📅 **Next 1-2 Months**
1. **#195** - Userland build system (after #192, #193)
2. **#29** - Complete toolchain integration
3. **#180, #185** - Environment-dependent shell features (✅ #188 complete)
4. **#181, #182** - Testing and validation

### 📅 **Next 3-6 Months**
1. **Threading**: #108 → #109 → #113 → #110 → #111
2. **IPC**: #102 → #103 → #104
3. **Shell UX**: #201 (mouse) - most UX features complete (tab completion ✅, Ctrl+R ✅)
4. **Native compilation**: Begin #190, #191

## 📈 Progress Assessment

### ✅ **Completed Recently**:
- Created 46 new issues and broke down complex IPC tasks
- Organized into categories with clear dependencies
- **Major completions**: #192 (crt0), #193 (libc), #148 (env vars), #180-#186 (shell milestone), #222 (cwd prompt), #37, #39 (sync primitives), #205 (I/O scheduler)

### ✅ **Completed Overall** (42 foundation issues! 🚀):
- **Foundation memory management**: #35, #57, #89
- **Core scheduling**: #34
- **Synchronization (COMPLETE!)**: #36 (mutex), #37 (semaphore), #39 (rwlock), #40 (condvar)
- **File I/O**: #96, #60, #65
- **Toolchain**: #192 (crt0), #193 (libc)
- **Shell milestone (9/9 core!)**: #148 (env vars), #180 (env seeding), #181 (tmpfs validation), #182 (waitpid tests), #183 (/bin utilities), #184 (line editor tests), #185 (PATH search), #186 (pipeline placeholders), #147 (getcwd/chdir) ✅
- **Threading foundation**: #108 (kernel threading)
- **IPC - Pipes (COMPLETE!)**: #102 (parent), #206 (pipe data structure), #207 (pipe syscall API), #208 (shell pipelines), #209 (named FIFOs)
- **IPC - Signals (5/6!)**: #103 (parent) ✅, #210 (signal bookkeeping), #211 (signal syscalls), #212 (signal delivery), #213 (shell Ctrl+C)
- **IPC - Shared Memory (COMPLETE!)**: #215 (shared mem manager), #216 (shared mem syscalls), #217 (reference counting), #218 (test suite), #219 (documentation)
- **Device Control (COMPLETE!)**: #220 (ioctl syscall)
- **Performance**: #205 (I/O scheduler)

### 🔥 **Ready to Implement** (9 issues - no dependencies!):
- **#194** - Syscall ABI docs (critical path)
- **#195** - Userland build system (dependencies met: #192✅, #193✅)
- **#188** - env utility ✅ (built-in /bin/env ships with default env dump)
- ✅ **#197** - Tab completion delivered using filesystem scanning
- **#213** - Shell Ctrl+C ✅ (dependencies met: #210✅, #211✅, #212✅)
- **#221** - Fast syscall instruction

### 📋 **Total Open Issues**: ~69 issues (estimated)
- **IPC**: 3 remaining (#105-#107, #214, #221)
  - Pipes: ✅ COMPLETE! (#102 + #206-#209 = 5/5) 🎉
  - Signals: 1 remaining (#214) - 5/6 complete! (#103, #210-#213) ✅✅✅✅✅ - Ctrl+C working!
  - Shared Memory: ✅ COMPLETE! (5/5) 🎉🎉🎉
  - Unix sockets: 1 (#105)
  - Microkernel IPC: 1 (#106)
  - Capability security: 1 (#107)
  - ioctl: ✅ COMPLETE! (1/1) 🎉
  - Fast syscalls: 1 (#221)
- **Shell & utilities**: 0 remaining in this bucket (ps/kill complete)
- **Shell UX**: 1 (#201) - history (#156) ✅, Ctrl+A/E (#198) ✅, Ctrl+L (#200) ✅, Ctrl+R (#199) ✅, pwd prompt (#222) ✅, tab completion (#197) ✅
- **Toolchain**: 4 remaining (#29, #194-#196) - 2/8 complete! ✅✅ (GCC milestone)
- **Threading**: 5 remaining (#109-#113) - 1/6 complete! ✅
- **Memory**: 1 (#95 userspace malloc)
- **Bug fixes**: 1 (#202 backlog)
- **Completed recently**: 43 issues (#192, #193, #148, #180-#188, #37, #39, #204, #205, #108, #206-#213, #215-#220, #147, #102, #103, #161, #164, #165, #187, #198, #199, #200, #203, #222, #156, #197)
- **Closed duplicates**: #157, #165
- **Previous existing**: ~64 issues

### 🎯 **New Issues by Category**:
- **Toolchain**: 7 issues (highest priority)
- **Shell**: 9 issues (polish & testing)
- **UX**: 5 issues (quality of life)
- **Bugs**: 2 issues (non-blocking)

## 💡 **Updated Strategy**

### Critical Path to Running Applications

```
Week 1-2:     #192 (crt0) + #194 (ABI docs)
Week 3-6:     #193 (libc) + #148 (env vars)
Week 7:       #195 (build system)
Week 8-10:    #29 (toolchain complete)
Week 11-12:   #183 (/bin utilities with new libc)
Week 13+:     Applications can be developed!
```

### Parallel Tracks

**Track A - Toolchain (Critical)**:
```
#192 → #193 → #195 → #29
```
**Duration**: 2-3 months
**Priority**: CRITICAL

**Track B - Shell Polish (Medium)**:
```
#183, #184, #186 (now) — #198 ✅, #200 ✅
↓
#148 → #180, #185, #188 (after env vars) ✅
```
**Duration**: 1-2 months
**Priority**: Medium

**Track C - Advanced Features (Long Term)**:
```
#189 (FAT32 write)
↓
#108 → #109 → #110 (threading)
↓
#102 → #103 → #104 (IPC)
```
**Duration**: 6-12 months
**Priority**: Medium-Low

## 🏆 **Key Achievements**

1. **Foundation Complete**: All core kernel infrastructure is done
2. **Shell Working**: Basic shell (mosh) functional with pipelines
3. **Clear Path Forward**: Toolchain is the critical path
4. **Organized Issues**: 87 issues now tracked and categorized
5. **Dependencies Mapped**: Clear dependency graph for planning

## 🎓 **Learning Opportunities**

For contributors, issues are organized by difficulty:

### Beginner-Friendly:
- ✅ #198 (Ctrl+A/E) - Simple keyboard shortcuts
- ✅ #200 (Ctrl+L) - Clear screen command
- #184 (line editor tests) - Writing tests

### Intermediate:
- #192 (crt0) - Assembly programming
- #183 (/bin utilities) - C programming
- #194 (ABI docs) - Technical writing

### Advanced:
- #193 (libc) - Systems programming
- #189 (FAT32 write) - File system development
- #108-#113 (threading) - Concurrency

## 📚 **Related Documentation**

- [Road to Shell](docs/road/road_to_shell.md) - Shell milestone details
- [Road to Doom](docs/road/road_to_doom.md) - Game porting roadmap
- [Road to GCC](docs/road/road_to_gcc.md) - Compilation roadmap
- [Issue Dependency Graph](issue_dependencies.png) - Visual diagram

## 🎯 **Success Metrics**

### Short Term (1-3 months):
- [ ] crt0 implemented (#192)
- [x] libc functional (#193)
- [ ] Toolchain complete (#29)
- [ ] Can compile simple C programs
- [ ] /bin utilities working

### Medium Term (3-6 months):
- [ ] Environment variables (#148, #180, #185) *(#188 complete)*
- [ ] Shell milestone complete
- [ ] FAT32 write support (#189)
- [ ] Threading foundation (#108, #109)

### Long Term (6-12 months):
- [ ] Full pthread API (#109-#113)
- [ ] Complete IPC (#102-#104)
- [ ] Native compilation (TCC) (#190)
- [ ] Advanced shell UX (#201) — tab completion (#197) ✅, Ctrl+R (#199) ✅ complete
- [ ] Running Doom!

---

**Next Review**: After #192 and #194 completion
**Contributors**: See [CONTRIBUTING.md](CONTRIBUTING.md)
**Questions**: Open an issue or discussion
