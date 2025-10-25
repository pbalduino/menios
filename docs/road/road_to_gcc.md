# Road to GCC: Native Compilation on meniOS

🎯 **Goal**: Enable native compilation of C programs on meniOS itself, starting with cross-compilation toolchain and eventually running GCC natively.

## 📊 **Progress Overview**

### **Phase 1: Cross-Compiler Toolchain** (Issue #29) ✅ **COMPLETE**

The foundation for all userland development. Without this, we can't build proper C programs for meniOS.

#### **Immediate Requirements**

| Component | Issue | Status | Priority |
| --- | --- | --- | --- |
| crt0 Runtime | #192 | ✅ DONE | Critical |
| Minimal libc | #193 | ✅ DONE | Critical |
| Syscall ABI Docs | #194 | ✅ DONE | High |
| Userland Build System | #195 | ✅ DONE | Critical |
| Cross-Compiler Integration | #29 | ✅ DONE | Critical |

Minimal libc now provides shared memory/string primitives, a simple `mmap`-backed heap (`malloc`/`free`/`aligned_alloc`), and base stdio (`printf`/`fprintf`/`sprintf`, `puts`, `perror`). The printf family now fully supports field width specifiers, zero-padding, precision, and alignment flags (#324).
crt0 runtime provides assembly startup stub that sets up argc/argv/envp and calls main().

`make userland` now builds this user-space stack (libc, crt0, and `/bin` utilities) independently of the kernel image, while `make build` consumes the resulting artifacts when assembling the disk.  The build prefers an `x86_64-elf` cross compiler (configurable via `MENIOS_CROSS_PREFIX` / `MENIOS_HOST_CC`) and gracefully falls back to the host compiler when the cross toolchain is unavailable.

**Dependencies:**
- ✅ #192 (crt0) - COMPLETE
- ✅ #193 (libc) - COMPLETE
- ✅ #194 (docs) - COMPLETE
- ✅ #195 (build) - COMPLETE
- ✅ #29 (toolchain integration) - COMPLETE

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
| Libc time conversions (gmtime/mktime/strftime) | #290 | ✅ DONE | Required for build system timestamps |
| Interval timers (`setitimer`) | #288 | ✅ DONE | Enables SIGALRM-based timeouts |
| File Write Support | #189 | ✅ COMPLETE | Compiler output requires writes |
| I/O Scheduler | #205 | ✅ DONE | Better performance for concurrent disk I/O |
| **Threading Foundation** | #108 | ✅ DONE | Kernel threading infrastructure |
| pthread API | #109 | ⛳ TODO | GCC uses threads (ready now!) |
| Thread-safe libc | #110-111 | ⛳ TODO | For multithreaded compilation |
| Fast Syscalls | #221 | ✅ DONE | syscall/sysret with proper 64-bit return values (#274 fixed) |

> **Update (v0.1.x)**: FAT32 now supports overwriting and creating short-name files (used for `/bin` updates). Long filenames, directory creation, and more advanced cluster management remain on the backlog (#189).

**Timeline Estimate:** 3-4 months

### **Phase 3: Userland Utilities & Libraries** 🚧 **IN PROGRESS**

Basic tools and library enhancements for testing the toolchain.

| Component | Issue | Status | Purpose |
| --- | --- | --- | --- |
| /bin utilities | #183 | ✅ DONE | echo, cat, env, true, false |
| PATH search | #185 | ✅ DONE | Command resolution |
| Process tools | #187 | ✅ Done | ps, kill |
| env utility | #188 | ✅ Done | Environment debugging (prints inherited variables) |

**Timeline Estimate:** 1-2 months

**Note**: scanf family (#304) was moved to Doom milestone as it's specifically needed for Doom config parsing.

### **Phase 4: Native Compilation (Long Term)** 🎯 **FUTURE**

Running compilers natively on meniOS.

| Component | Issue | Status | Complexity | Priority |
| --- | --- | --- | --- | --- |
| TCC Port | #190 | 🚀 READY | Medium | Nice to have |
| binutils Port | #191 | 🚀 READY | High | Nice to have |
| GCC Port | - | ⛳ TODO | Very High | Future |

The project vendors [Tiny C Compiler (TCC) 0.9.24](https://bellard.org/tcc/) under `vendor/tcc-0.9.24/`; those sources will be the starting point for issue #190.

GNU [binutils 2.45](https://www.gnu.org/software/binutils/) has also been staged under `vendor/binutils-2.45/`. The upstream tree keeps each major component in its own directory (`binutils/` for user-facing tools, `bfd/` for the Binary File Descriptor library, `opcodes/` for disassembler tables, and `ld/` for the linker). Building it requires the usual GNU autotools flow (POSIX shell, `make`, a cross compiler targeting `x86_64-menios`—for now either `x86_64-elf-gcc` or `clang` with `-target x86_64-unknown-elf` paired with `lld`). For meniOS we plan to configure with options such as `--disable-nls`, `--disable-gdb`, `--disable-gprof`, `--disable-libdecnumber`, and `--disable-gold` to avoid pulling in unsupported libc features.

**Dependencies for TCC (#190):**
- ✅ #29 (cross-compiler complete) - COMPLETE
- ✅ #193 (libc) - COMPLETE
- ✅ #337 (signal API) - **COMPLETE!** (was TCC blocker)
- ✅ #338 (floating-point parsing helpers: strtod/strtof; strtold currently aliases to strtod) - **COMPLETE!** (was TCC blocker)
- ⚠️ #364 (stubbed libc functions) - **PARTIALLY COMPLETE** (stat family working, other stubs tracked)
  - ✅ stat/fstat/lstat syscalls (SYS_STAT, SYS_LSTAT, SYS_FSTAT)
  - ✅ access(), realpath() (using stat infrastructure)
  - ⚠️ pathconf() (partial - only _PC_PATH_MAX) - #368
  - ❌ chmod/fchmod (#365), utime (#317) - file mutation APIs
  - ❌ pseudo-fs metadata (#366), rich FAT32 metadata (#367)
  - ❌ isatty (#347), brk/sbrk (#21), system() (#369), timing APIs (#327)
- ✅ Pipes & FIFOs - **FULLY COMPLETE!** (#206 ✅, #207 ✅, #208 ✅, #209 ✅)
- ✅ #189 (file writes) - **COMPLETE!** (#291, #292, #293 all done)
- ✅ #294 (VFS streaming I/O) - **COMPLETE!** (#295, #296, #297, #298 all done)
- ✅ #205 (I/O scheduler) - COMPLETE
- **🎉 ALL CRITICAL DEPENDENCIES MET - TCC BLOCKERS RESOLVED!**
- **📝 Note:** Remaining stubbed functions (#364 sub-issues) are nice-to-have for full POSIX compliance but don't block TCC

**Dependencies for binutils (#191):**
- ✅ #29 (cross-compiler complete) - COMPLETE
- ✅ #193 (libc) - COMPLETE
- ✅ #189 (file writes) - **COMPLETE!** (#291, #292, #293 all done)
- ✅ #294 (VFS streaming I/O) - **COMPLETE!** (#295, #296, #297, #298 all done)
- ✅ #205 (I/O scheduler) - COMPLETE
- **🎉 ALL DEPENDENCIES MET - ZERO BLOCKERS!**

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

4. ✅ **#195** - Setup build system (1 week)
   - Dedicated `make userland` target and SDK integration

5. ✅ **#29** - Integration testing (1-2 weeks)
   - Cross toolchain builds `/bin` programs via `make userland`
   - Tested through the existing `/bin` suite and disk image build

**Total Estimated Time:** Complete — cross-compiler flow is live via `make userland` and `menios-gcc`.

## 🎯 **Critical Path to Native Compilation**

Much longer path, requires most of the OS to work:

```
Cross-Compiler (#29) — ✅ complete
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

### ✅ Week 7-8: Integration
- [x] **#195** - Create userland Makefile / targets
- [x] **#195** - Setup proper CFLAGS
- [x] **#29** - Test with simple programs

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

The GCC milestone on GitHub now tracks 9 issues:
- **Status**: 7/9 complete (77.8%)
- **Completed**:
  - #192 (crt0 runtime) ✅
  - #193 (libc foundation) ✅
  - #194 (syscall ABI docs) ✅
  - #195 (build system) ✅
  - #29 (cross-compiler integration) ✅
- **In Progress**:
  - #190 (TCC port) - READY TO START, all dependencies met!
  - #191 (binutils port) - READY TO START, all dependencies met!

See [MILESTONES.md](../MILESTONES.md) for detailed milestone tracking across all three major goals (Mosh, GCC, Doom).

---

**Last Updated**: 2025-10-20
**Status**: 🚀 **Phase 4 Ready - 7/9 issues complete (77.8%)**
**Current Release**: v0.1.666 "DOOM READY" includes complete cross-compiler toolchain and comprehensive libc
**Ready to Start**: TCC (#190) and binutils (#191) - all dependencies complete!
