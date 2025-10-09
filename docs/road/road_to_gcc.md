# Road to GCC: Native Compilation on meniOS

🎯 **Goal**: Enable native compilation of C programs on meniOS itself, starting with cross-compilation toolchain and eventually running GCC natively.

## 📊 **Progress Overview**

### **Phase 1: Cross-Compiler Toolchain** (Issue #29) 🚧 **IN PROGRESS**

The foundation for all userland development. Without this, we can't build proper C programs for meniOS.

#### **Immediate Requirements**

| Component | Issue | Status | Priority |
| --- | --- | --- | --- |
| crt0 Runtime | #192 | ✅ DONE | Critical |
| Minimal libc | #193 | ✅ DONE | Critical |
| Syscall ABI Docs | #194 | ✅ DONE | High |
| Userland Build System | #195 | ⛳ TODO | Critical |

Minimal libc now provides shared memory/string primitives, a simple `mmap`-backed heap (`malloc`/`free`/`aligned_alloc`), and base stdio (`printf`/`fprintf`/`sprintf`, `puts`, `perror`).
crt0 runtime provides assembly startup stub that sets up argc/argv/envp and calls main().

**Dependencies:**
- ✅ #192 (crt0) - COMPLETE
- ✅ #193 (libc) - COMPLETE
- ✅ #194 (docs) - COMPLETE
- #195 (build) - Can start now (dependencies met: #192✅, #193✅)

**Timeline Estimate:** 2-3 months

### **Phase 2: System Services for Compilation** 🔄 **PARTIALLY COMPLETE**

Services needed for any compiler to function properly.

| Component | Issue | Status | Notes |
| --- | --- | --- | --- |
| **Pipes & FIFOs** (broken down) | #206-#209 | ✅ COMPLETE | GCC uses pipes between stages |
| • Pipe data structure | #206 | ✅ DONE | Kernel control path |
| • Pipe syscall API | #207 | ✅ DONE | pipe() syscall |
| • Shell pipelines | #208 | ✅ DONE | Shell integration |
| • Named FIFOs | #209 | ✅ DONE | mkfifo support |
| **Signals** (broken down) | #210-#214 | 🔄 In progress (3/5) | For interrupt handling |
| • Signal bookkeeping | #210 | ✅ DONE | Kernel infrastructure |
| • Signal syscalls | #211 | ✅ DONE | kill(), sigaction() |
| • Signal delivery | #212 | ✅ DONE | User handlers |
| • Shell Ctrl+C | #213 | ✅ Done | Process control |
| File Write Support | #189 | ⛳ TODO | Compiler output requires writes |
| I/O Scheduler | #205 | ✅ DONE | Better performance for concurrent disk I/O |
| **Threading Foundation** | #108 | ✅ DONE | Kernel threading infrastructure |
| pthread API | #109 | ⛳ TODO | GCC uses threads (ready now!) |
| Thread-safe libc | #110-111 | ⛳ TODO | For multithreaded compilation |
| Fast Syscalls | #221 | ⛳ TODO | 3-5x faster syscall performance |

**Timeline Estimate:** 3-4 months

### **Phase 3: Userland Utilities** 🚧 **IN PROGRESS**

Basic tools for testing the toolchain.

| Component | Issue | Status | Purpose |
| --- | --- | --- | --- |
| /bin utilities | #183 | ✅ DONE | echo, cat, env, true, false |
| PATH search | #185 | ✅ DONE | Command resolution |
| Process tools | #187 | ✅ Done | ps, kill |
| env utility | #188 | ✅ Done | Environment debugging (prints inherited variables) |

**Timeline Estimate:** 1-2 months

### **Phase 4: Native Compilation (Long Term)** 🎯 **FUTURE**

Running compilers natively on meniOS.

| Component | Issue | Status | Complexity | Priority |
| --- | --- | --- | --- | --- |
| TCC Port | #190 | ⛳ TODO | Medium | Nice to have |
| binutils Port | #191 | ⛳ TODO | High | Nice to have |
| GCC Port | - | ⛳ TODO | Very High | Future |

**Dependencies for TCC (#190):**
- #29 (cross-compiler complete)
- ✅ #193 (libc) - COMPLETE
- ✅ Pipes & FIFOs - **FULLY COMPLETE!** (#206 ✅, #207 ✅, #208 ✅, #209 ✅)
- #189 (file writes)
- ✅ #205 (I/O scheduler) - COMPLETE - Performance boost available!

**Dependencies for binutils (#191):**
- #29 (cross-compiler complete)
- ✅ #193 (libc) - COMPLETE
- #189 (file writes)
- ✅ #205 (I/O scheduler) - COMPLETE - Performance boost available!

**Timeline Estimate:** 6-12 months

## 🛣️ **Critical Path to Cross-Compilation**

The **shortest path** to compiling C programs for meniOS:

1. ✅ **#192** - Implement crt0 (1 week) - COMPLETE
   - Assembly stub that calls main() and handles argc/argv/envp
   - No dependencies

2. ✅ **#194** - Document syscall ABI (published in `docs/architecture/syscall_abi.md`)
   - Reference documentation is now available for toolchain work
   - No remaining dependencies

3. ✅ **#193** - Build libc (3-4 weeks) - COMPLETE
   - Syscall wrappers, string/memory primitives, and basic stdio now ship with `libmeniosc`.

4. **#195** - Setup build system (1 week, can start now!)
   - Dependencies met: #192✅, #193✅
   - Integrate into Makefile

5. **#29** - Integration testing (1-2 weeks)
   - Requires: #194, #195
   - Compile test programs
   - Fix issues

**Total Estimated Time:** ~2 weeks remaining for cross-compiler (just #194, #195, #29!)

## 🎯 **Critical Path to Native Compilation**

Much longer path, requires most of the OS to work:

```
Cross-Compiler (#29) — Just #194 + #195 away!
    ↓
✅ Environment Variables (#148) - COMPLETE
    ↓
Pipes (#207-#208) + Signals (#210-#214) + pthread (#109)
    ↓ (Note: #206 ✅ complete, #108 ✅ complete)
File Write Support (#189)
    ↓
Comprehensive libc with thread safety (#110)
    ↓
TCC Port (#190) — Lightweight compiler
    ↓
binutils Port (#191) — as, ld
    ↓
Test native compilation workflow
    ↓
(Eventually) GCC Port — Full compiler suite
```

**Total Estimated Time:** 12-18 months for native GCC

## 📋 **Immediate Action Items**

### ✅ Week 1-2: Foundation - COMPLETE!
- [x] **#192** - Write crt0.S assembly ✅
- [x] **#194** - Document all syscalls and ABI ✅

### ✅ Week 3-6: Core Library - COMPLETE!
- [x] **#193** - Implement syscall wrappers ✅
- [x] **#193** - Implement string functions ✅
- [x] **#193** - Implement malloc/free ✅
- [x] **#193** - Implement printf family ✅

### Week 7-8: Integration (READY NOW!)
- [ ] **#195** - Create userland Makefile (deps met!)
- [ ] **#195** - Setup proper CFLAGS
- [ ] **#29** - Test with simple programs

### Week 9-10: Validation
- [x] **#183** - Rewrite /bin utilities using new libc ✅
- [x] **#185** - PATH search configuration ✅
- [x] **#187** - Implement ps/kill with new libc ✅
- [x] **#188** - Implement env with new libc (prints inherited environment)

## 🎓 **Why This Matters**

### For meniOS Development
- **Productivity**: Stop reimplementing syscalls in every program
- **Compatibility**: Programs can use standard C APIs
- **Testing**: Easier to write comprehensive test suites
- **Quality**: Standard library reduces bugs

### For Educational Value
- Demonstrates complete OS development lifecycle
- Shows cross-compilation toolchain setup
- Illustrates userland/kernel separation
- Real-world development practices

### For Future Goals
- **Required for Doom**: Doom needs full libc
- **Enables Porting**: Other software can be ported
- **Native Development**: Eventually compile on meniOS itself
- **Self-Hosting**: Ultimate goal of any OS

## 🔍 **Success Criteria**

### Cross-Compiler (Phase 1) Complete When:
- ✅ Can write `int main() { return 0; }` and it works
- ✅ Can use `printf("Hello, World!\n")`
- ✅ Can use `malloc()` and `free()`
- ✅ Can open/read/write files with standard C APIs
- ✅ All /bin utilities use shared libc

### Native Compilation (Phase 4) Complete When:
- ✅ TCC runs on meniOS and compiles programs
- ✅ `as` and `ld` work natively
- ✅ Can compile a program entirely on meniOS
- ✅ Generated binaries execute correctly

## 📚 **References**

### Relevant Documentation
- [Road to Shell](./road_to_shell.md) - Shell milestone requirements
- [Road to Doom](./road_to_doom.md) - Game porting requirements
- [Cross-Compiler Toolchain (#29)](https://github.com/pbalduino/menios/issues/29)

### Similar Projects
- **Linux From Scratch**: Cross-compilation techniques
- **OSDev Wiki**: Bare-metal C library implementation
- **musl libc**: Minimal, clean libc implementation
- **newlib**: Embedded systems C library

## 🚀 **Getting Started**

### For Contributors

Start with the most critical issues:

1. ✅ **crt0 (#192)** - COMPLETE
2. **Syscall docs (#194)** - Ready now! Documentation task
3. ✅ **libc (#193)** - COMPLETE
4. **Build system (#195)** - Ready now! (deps met)

Each component can be developed somewhat independently, then integrated together.

### Testing Strategy

1. **Unit Tests**: Test each libc function individually
2. **Integration Tests**: Test linking with crt0
3. **System Tests**: Compile and run complete programs
4. **Regression Tests**: Ensure utilities still work

## 🎉 **Long-Term Vision**

When complete, developers will:

1. Write standard C programs with no OS-specific cruft
2. Use familiar APIs (POSIX-style)
3. Link against a robust, tested libc
4. Eventually compile programs natively on meniOS

This transforms meniOS from a kernel project into a true operating system with a complete development environment.

---

## 🎯 **GitHub Milestone Tracking**

The GCC milestone on GitHub now tracks 8 issues:
- **Status**: 2/8 complete (25%)
- **Completed**: #192 (crt0) ✅, #193 (libc) ✅
- **In Progress**: #194 (ABI docs), #195 (build system), #29 (toolchain), #190 (TCC), #191 (binutils), #196 (Fish research)

See [MILESTONES.md](../MILESTONES.md) for detailed milestone tracking across all three major goals (Mosh, GCC, Doom).

---

**Last Updated**: 2025-10-08
**Next Review**: After #194 and #195 completion
