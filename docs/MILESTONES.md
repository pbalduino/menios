# meniOS Milestones

This document tracks the three major milestones for meniOS development.

## 📊 Milestone Overview

### 1. **Mosh** (Shell Milestone)
**Goal**: Complete interactive shell with full user experience
**GitHub Milestone**: [Mosh](https://github.com/pbalduino/menios/milestone/1)

**Status**: 1/10 complete (10%)

**Assigned Issues**:
- [x] #185 - PATH search configuration ✅
- [ ] #147 - getcwd/chdir syscalls
- [ ] #187 - /bin/ps and /bin/kill utilities
- [ ] #188 - /bin/env utility
- [ ] #197 - Tab completion for files/directories
- [ ] #198 - Ctrl+A/E line editing shortcuts
- [ ] #199 - Ctrl+R reverse search
- [ ] #200 - Ctrl+L clear screen
- [ ] #201 - Mouse selection/copy/paste
- [ ] #222 - Current directory in prompt

**Dependencies**:
- #187 depends on #213 (signal support for kill)
- #197 depends on #147 (getcwd/chdir)
- #222 depends on #147 (getcwd/chdir)

---

### 2. **GCC** (Toolchain Milestone)
**Goal**: Enable native compilation on meniOS with GCC toolchain support
**GitHub Milestone**: [GCC](https://github.com/pbalduino/menios/milestone/2)

**Status**: 0/6 complete (0%)

**Assigned Issues**:
- [ ] #194 - Syscall ABI documentation (ready now!)
- [ ] #195 - Userland build system (ready now!)
- [ ] #29 - Cross-compiler toolchain integration
- [ ] #190 - TCC (Tiny C Compiler) port
- [ ] #191 - binutils (as, ld) port
- [ ] #196 - Fish shell research

**Critical Path**: #194 → #195 → #29 → #190/#191

**Dependencies**:
- #29 requires #194, #195
- #190 requires #29, #189 (FAT32 writes)
- #191 requires #29, #189 (FAT32 writes)

---

### 3. **Doom** (Game Porting Milestone)
**Goal**: Run Doom (1993) in userland on meniOS
**GitHub Milestone**: [Doom](https://github.com/pbalduino/menios/milestone/3)

**Status**: 10/20 complete (50%)

**Assigned Issues**:

#### Graphics & Audio
- [ ] #31 - Userspace graphics interface
- [ ] #32 - Input subsystem
- [ ] #33 - Audio subsystem

#### Threading Support (5 issues)
- [ ] #109 - pthread API implementation (ready now!)
- [ ] #110 - Thread-safe C library
- [ ] #111 - Advanced pthread synchronization
- [ ] #112 - Thread debugging and profiling
- [ ] #113 - Thread-aware system calls

#### File System
- [ ] #189 - FAT32 write support

#### IPC - Signals (5 issues)
- [x] #210 - Signal bookkeeping scaffold ✅
- [x] #211 - Signal syscalls ✅
- [x] #212 - Signal delivery path ✅
- [ ] #213 - Shell Ctrl+C integration (ready now!)
- [ ] #214 - Advanced signal features

#### IPC - Shared Memory (5 issues) ✅ COMPLETE!
- [x] #215 - Shared memory manager ✅
- [x] #216 - Shared memory syscalls ✅
- [x] #217 - Reference counting and cleanup ✅
- [x] #218 - Comprehensive test suite ✅
- [x] #219 - Documentation and examples ✅

#### Other IPC
- [x] #220 - ioctl syscall ✅
- [ ] #221 - Fast syscall instruction (ready now!)

**Dependencies**:
- #110 requires #109
- #111 requires #109
- #212 requires #211
- #213 requires #212
- #214 requires #213, #109

---

## 🎯 Dependency Flow Between Milestones

```
┌─────────────────────────────────────────┐
│  Mosh (Shell UX & Developer Tools)      │
│  - Interactive shell complete            │
│  - Basic utilities working               │
│  - Development environment ready         │
└────────────────┬────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────┐
│  GCC (Native Compilation Toolchain)      │
│  - Cross-compiler working                │
│  - Can compile C programs for meniOS     │
│  - Eventually native compilation         │
└────────────────┬────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────┐
│  Doom (Full OS Capabilities)             │
│  - Graphics, audio, threading            │
│  - Complete IPC support                  │
│  - Can run complex userland applications │
└─────────────────────────────────────────┘
```

## 📈 Overall Progress

- **Total Issues Across Milestones**: 36 issues
- **Completed**: 11 issues (30.6%)
- **In Progress**: 25 issues
- **Ready to Start**: 7 issues (no dependencies)

## 🚀 Immediate Next Steps

### Ready to Start Now (No Dependencies):
1. **GCC Milestone**:
   - #194 - Syscall ABI docs
   - #195 - Userland build system

2. **Doom Milestone**:
   - #109 - pthread API
   - #213 - Shell Ctrl+C integration
   - #221 - Fast syscall instruction

3. **Mosh Milestone**:
   - #188 - /bin/env utility
   - #198 - Ctrl+A/E shortcuts
   - #200 - Ctrl+L clear screen

## 📝 Notes

### Parallel Development
Many issues can be worked on in parallel:
- **GCC**: #194 and #195 can be done simultaneously
- **Doom Threading**: #109, #112, #113 are independent
- **Doom IPC**: Signals (#211-#214), Shared Memory (#215-#219), and Other (#220, #221) can progress in parallel
- **Mosh UX**: #198, #199, #200 are independent features

### Critical Dependencies
- **GCC milestone** is required before meniOS can compile Doom natively
- **Mosh milestone** provides the development environment for debugging
- Many **Doom** features are prerequisites for running the game

### Completion Order
Recommended completion order for maximum impact:
1. **Mosh** - Provides usable development environment
2. **GCC** - Enables native development and compilation
3. **Doom** - Demonstrates full OS capabilities

---

**Last Updated**: 2025-10-08
**See Also**:
- [Road to Shell](docs/road/road_to_shell.md)
- [Road to GCC](docs/road/road_to_gcc.md)
- [Road to Doom](docs/road/road_to_doom.md)
