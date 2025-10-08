# Road to GCC: Native Compilation on meniOS

🎯 **Goal**: Enable native compilation of C programs on meniOS itself, starting with cross-compilation toolchain and eventually running GCC natively.

## 📊 **Progress Overview**

### **Phase 1: Cross-Compiler Toolchain** (Issue #29) 🚧 **IN PROGRESS**

The foundation for all userland development. Without this, we can't build proper C programs for meniOS.

#### **Immediate Requirements**

| Component | Issue | Status | Priority |
| --- | --- | --- | --- |
| crt0 Runtime | #192 | ⛳ TODO | Critical |
| Minimal libc | #193 | ✅ DONE | Critical |
| Syscall ABI Docs | #194 | ⛳ TODO | High |
| Userland Build System | #195 | ⛳ TODO | Critical |

Minimal libc now provides shared memory/string primitives, a simple `mmap`-backed heap (`malloc`/`free`/`aligned_alloc`), and base stdio (`printf`/`fprintf`/`sprintf`, `puts`, `perror`).

**Dependencies:**
- #192 (crt0) - No dependencies, can start now
- #193 (libc) - Depends on #148 (environment variables), #192 (crt0)
- #194 (docs) - Can start now
- #195 (build) - Depends on #192, #193

**Timeline Estimate:** 2-3 months

### **Phase 2: System Services for Compilation** 🔄 **PARTIALLY COMPLETE**

Services needed for any compiler to function properly.

| Component | Issue | Status | Notes |
| --- | --- | --- | --- |
| Pipes/FIFOs | #102 | ⛳ TODO | GCC uses pipes between stages |
| UNIX Signals | #103 | ⛳ TODO | For interrupt handling |
| File Write Support | #189 | ⛳ TODO | Compiler output requires writes |
| I/O Scheduler | #205 | ⛳ TODO | Better performance for concurrent disk I/O |
| Threading | #109-111 | ⛳ TODO | GCC uses threads for optimization |

**Timeline Estimate:** 3-4 months

### **Phase 3: Userland Utilities** 🚧 **IN PROGRESS**

Basic tools for testing the toolchain.

| Component | Issue | Status | Purpose |
| --- | --- | --- | --- |
| /bin utilities | #183 | ⛳ TODO | echo, cat, env, true, false |
| Process tools | #187 | ⛳ TODO | ps, kill |
| env utility | #188 | ⛳ TODO | Environment debugging |

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
- #193 (libc)
- #102 (pipes)
- #189 (file writes)
- #205 (I/O scheduler - optional but beneficial)

**Dependencies for binutils (#191):**
- #29 (cross-compiler complete)
- #193 (libc)
- #189 (file writes)
- #205 (I/O scheduler - optional but beneficial)

**Timeline Estimate:** 6-12 months

## 🛣️ **Critical Path to Cross-Compilation**

The **shortest path** to compiling C programs for meniOS:

1. **#192** - Implement crt0 (1 week)
   - Assembly stub that calls main()
   - No dependencies

2. **#194** - Document syscall ABI (1 week, parallel with #192)
   - Reference documentation
   - No dependencies

3. **#193** - Build libc (3-4 weeks) ✅ *Completed*
  - Syscall wrappers, string/memory primitives, and basic stdio now ship with `libmeniosc`.

4. **#195** - Setup build system (1 week)
   - Requires #192, #193
   - Integrate into Makefile

5. **#29** - Integration testing (1-2 weeks)
   - Compile test programs
   - Fix issues

**Total Estimated Time:** 2-3 months for cross-compiler

## 🎯 **Critical Path to Native Compilation**

Much longer path, requires most of the OS to work:

```
Cross-Compiler (#29)
    ↓
Environment Variables (#148)
    ↓
Pipes (#102) + Signals (#103) + Threading (#109-111)
    ↓
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

### Week 1-2: Foundation
- [ ] **#192** - Write crt0.S assembly
- [ ] **#194** - Document all syscalls and ABI

### Week 3-6: Core Library
- [x] **#193** - Implement syscall wrappers
- [x] **#193** - Implement string functions
- [x] **#193** - Implement malloc/free
- [x] **#193** - Implement printf family

### Week 7-8: Integration
- [ ] **#195** - Create userland Makefile
- [ ] **#195** - Setup proper CFLAGS
- [ ] **#29** - Test with simple programs

### Week 9-10: Validation
- [ ] **#183** - Rewrite /bin utilities using new libc
- [ ] **#187** - Implement ps/kill with new libc
- [ ] **#188** - Implement env with new libc

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

1. **crt0 (#192)** - Good first issue, pure assembly
2. **Syscall docs (#194)** - Documentation task
3. **libc (#193)** - Large task, can be split up

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

**Last Updated**: 2025-10-05
**Next Review**: After #192 (crt0) completion
