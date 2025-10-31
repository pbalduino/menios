# Issue Dependency Diagrams

The meniOS project has grown to over 100+ tracked issues with complex interdependencies. To maintain readability, the dependency visualization has been split into multiple focused diagrams.

## Available Diagrams

### 1. Critical Path (`issue_dependencies_critical.png`) 🔴

**Focus:** The critical path to TCC/GCC native compilation

**Size:** 237KB (compact and readable)

**Shows:**
- Foundation issues (all complete ✅)
- TCC blockers: #337 (Signal API ✅), #338 (Float parsing ✅) - **COMPLETE!**
- TCC port: #190 - **READY TO START!**
- binutils port: #191 - **BLOCKED BY #371**
- Bug blocker: #371 (ld freeze) - **CRITICAL**
- Timeline: TCC ready to start, binutils blocked pending bug fix

**Use this for:**
- Understanding what's blocking native compilation
- Prioritizing toolchain work
- Tracking progress to TCC milestone

**Key Insights:**
- ✅ TCC has zero blockers - ready to start!
- ⚠️ binutils build complete but blocked by ld freeze bug (#371)
- 🚨 #371 is the critical blocker for native compilation

---

### 2. libc Ecosystem (`issue_dependencies_libc.png`) 📚

**Focus:** All C standard library implementation work

**Size:** 494KB

**Shows:**
- Foundation: Minimal libc #193 (complete ✅)
- Doom libc gaps: #304-#310 (all complete ✅)
- **TCC blockers:** #337 (Signal API ✅ CLOSED), #338 (Float parsing ✅ CLOSED)
- **Stubbed functions:** #364 (parent), #367-#369, #347, #21, #327
  - ✅ stat/fstat/lstat (complete for FAT32, skeletal)
  - ✅ access(), realpath() (complete)
  - ⚠️ pathconf() (partial - #368)
  - ✅ chmod/fchmod (#365); ✅ utime (#317)
  - ✅ pseudo-fs metadata (#366); ✅ rich FAT32 metadata (#367)
  - ❌ isatty (#347), brk/sbrk (#21), system() (#369)
  - ❌ timing APIs (#327)
- Thread safety: #339 (depends on pthread #109)
- Extended features: stdio, math, regex, TTY helpers, multiplexing
- Future work: sockets, locale, wide-char, dynamic loader

**Use this for:**
- Understanding libc completeness
- Planning POSIX compliance work
- Identifying gaps for specific programs

**Key Categories:**
- ✅ **Complete:** Basic libc, Doom gaps, Signal API, Float parsing
- 🟡 **Stubbed Functions:** #364 tracking (partially complete - stat family done)
- 🔵 **Threading:** Thread-safe libc (after pthread)
- 🟡 **Extended:** Additional POSIX features
- 🟣 **Future:** Nice-to-have features

**Recent Progress:**
- ✅ #337 (Signal API) - CLOSED
- ✅ #338 (Float parsing) - CLOSED - TCC blockers resolved! 🎉
- ✅ #371 (ld freeze bug) - **CLOSED** - as and ld working! 🎉
- ✅ #373 (realpath command) - **CLOSED** - Quick win complete! 🎉
- ⚠️ #364 (Stubbed functions) - Partially complete (stat family working)
- ✅ #370 (stat command) - **CLOSED** - File metadata tool now ships in `/bin/stat`
- 🆕 #372 (shutdown command) - NEW - Clean ACPI power-off from userland (quick win)
- ✅ #191 (binutils) - Build complete, **READY FOR TESTING** (unblocked)

**Recently Closed (19 issues):**
- #58, #137, #138, #145, #149, #151, #152, #153, #155, #167, #168, #226, #284, #285, #316, #337, #338, #371, #373

---

### 3. Hardware Infrastructure (`issue_dependencies_hardware.png`) ⚡

**Focus:** ACPI, PCI, device drivers, audio, and networking

**Size:** ~250KB

**Shows:**
- PCI infrastructure: #335, #280 (complete ✅)
- UACPI integration: #276 (73% complete, 16/22 functions)
- **Missing infrastructure:**
  - #278 (Work queue) - blocks UACPI
  - #279 (Dynamic IRQ) - **CRITICAL** blocker - blocks 5 issues
  - #281 (I/O port mgmt) - blocks UACPI
- Drivers:
  - #67 (e1000 network) - blocked by #279
  - #336 (termios) - ready to implement ✅
- **Audio subsystem (Doom):**
  - #33 (parent) - blocked by #279
  - #382 (AC'97 driver) - blocked by #279
  - #383 (kernel audio core) - blocked by #279
- Networking stack (future)

**Use this for:**
- Hardware bring-up planning
- Driver development priorities
- Understanding ACPI/UACPI status
- Audio subsystem dependencies

**Key Insight:** #279 (Dynamic IRQ) is the **HIGHEST PRIORITY** blocker - blocks 5 open issues:
- #276 (UACPI interrupt handlers)
- #67 (e1000 network driver)
- #382 (AC'97 audio driver)
- #383 (Kernel audio core)
- #33 (Audio subsystem - Doom milestone)

---

### 4. Full Diagram (`issue_dependencies_full.png`) 📊

**Focus:** Complete dependency graph (all issues)

**Size:** 894KB (large and complex)

**Shows:** Everything - all 100+ issues with all dependencies

**Use this for:**
- Reference/archive purposes
- Comprehensive dependency analysis
- When you need to see the full picture

**Note:** This diagram is intentionally complex and may have overlapping elements. Use the focused diagrams above for specific areas.

---

## Quick Reference

| Diagram | Size | Issues | Best For |
|---------|------|--------|----------|
| **Critical** | 237KB | ~15 | TCC/toolchain path |
| **libc** | 494KB | ~30 | Library implementation |
| **Hardware** | 239KB | ~15 | Drivers & ACPI |
| **Full** | 894KB | 100+ | Complete reference |

## Color Coding

All diagrams use consistent color coding:

- 🟢 **Green (dashed):** Complete ✅
- 🔴 **Red/Pink:** Critical blockers ⚠️
- 🔵 **Blue:** Threading/concurrency
- 🟡 **Yellow/Orange:** Medium priority / In progress
- 🟣 **Purple:** Future / Nice to have
- ⚪ **Gray:** Background/foundation

## Source Files

Diagrams are generated from Graphviz `.dot` files:

- `issue_dependencies_critical.dot` → `.png`
- `issue_dependencies_libc.dot` → `.png`
- `issue_dependencies_hardware.dot` → `.png`
- `issue_dependencies_full.dot` → `.png`

To regenerate:
```bash
cd docs
dot -Tpng issue_dependencies_critical.dot -o issue_dependencies_critical.png
dot -Tpng issue_dependencies_libc.dot -o issue_dependencies_libc.png
dot -Tpng issue_dependencies_hardware.dot -o issue_dependencies_hardware.png
sfdp -Tpng issue_dependencies_full.dot -o issue_dependencies_full.png
```

**Note:** The full diagram uses `sfdp` instead of `dot` for better force-directed layout.

## Maintenance

When updating diagrams:

1. **Add new issues** to the appropriate diagram(s)
2. **Update status** when issues are completed (change to green/dashed)
3. **Add dependencies** as edges between nodes
4. **Regenerate PNGs** using the commands above
5. **Update this README** if adding new diagram categories

## Related Documentation

- [ISSUE_DEPENDENCY_ANALYSIS.md](ISSUE_DEPENDENCY_ANALYSIS.md) - Detailed text analysis
- [MILESTONES.md](MILESTONES.md) - Project milestones
- [Road to GCC](road/road_to_gcc.md) - Compilation roadmap
- [Road to Doom](road/road_to_doom.md) - Gaming milestone

---

**Last Updated:** 2025-10-31
**Total Issues Tracked:** 100+
**Diagrams:** 4 (3 focused + 1 complete)

---

## Critical Blocking Issue: #279

**Issue #279 (Dynamic IRQ handler registration)** is currently the highest priority blocker in the project, blocking 5 open issues across 3 major subsystems:

### Blocked Issues

1. **#276** - UACPI kernel interface (ACPI System Control Interrupt handling)
2. **#67** - e1000 network driver (TX/RX interrupts)
3. **#382** - AC'97 audio driver (DMA buffer completion interrupts)
4. **#383** - Kernel audio core (interrupt notification path)
5. **#33** - Audio subsystem parent (blocks entire audio stack for Doom)

### Impact

Without #279:
- ❌ No ACPI event handling (power button, thermal events)
- ❌ No interrupt-driven networking (must use inefficient polling)
- ❌ No audio subsystem (blocks Doom audio milestone)
- ❌ Cannot achieve acceptable audio latency

### Recommendation

**Prioritize #279 as the highest priority task** to unblock:
- ACPI infrastructure (1 issue)
- Networking stack (1 issue)
- Audio subsystem (3 issues + Doom milestone)
