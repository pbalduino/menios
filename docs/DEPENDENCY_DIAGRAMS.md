# Issue Dependency Diagrams

The meniOS project has grown to over 100+ tracked issues with complex interdependencies. To maintain readability, the dependency visualization has been split into multiple focused diagrams.

## Available Diagrams

### 1. Critical Path (`issue_dependencies_critical.png`) 🔴

**Focus:** The critical path to TCC/GCC native compilation

**Size:** 174KB (compact and readable)

**Shows:**
- Foundation issues (all complete ✅)
- TCC blockers: #337 (Signal API), #338 (Float parsing)
- TCC port: #190
- binutils port: #191
- Timeline: 19-26 weeks to TCC

**Use this for:**
- Understanding what's blocking native compilation
- Prioritizing toolchain work
- Tracking progress to TCC milestone

**Key Insight:** Only 2 issues block TCC (#337, #338), both are libc work with clear timelines.

---

### 2. libc Ecosystem (`issue_dependencies_libc.png`) 📚

**Focus:** All C standard library implementation work

**Size:** 293KB

**Shows:**
- Foundation: Minimal libc #193 (complete ✅)
- Doom libc gaps: #304-#310 (all complete ✅)
- **TCC blockers:** #337 (Signal API ✅ CLOSED), #338 (Float parsing ✅ CLOSED)
- **Stubbed functions:** #364 (parent), #365-#369, #317, #347, #21, #327
  - ✅ stat/fstat/lstat (complete for FAT32, skeletal)
  - ✅ access(), realpath() (complete)
  - ⚠️ pathconf() (partial - #368)
  - ❌ chmod/fchmod (#365), utime (#317)
  - ❌ pseudo-fs metadata (#366), rich FAT32 metadata (#367)
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
- ⚠️ #364 (Stubbed functions) - Partially complete (stat family working)

---

### 3. Hardware Infrastructure (`issue_dependencies_hardware.png`) ⚡

**Focus:** ACPI, PCI, device drivers, and networking

**Size:** 239KB

**Shows:**
- PCI infrastructure: #335, #280 (complete ✅)
- UACPI integration: #276 (73% complete, 16/22 functions)
- **Missing infrastructure:**
  - #278 (Work queue) - blocks UACPI
  - #279 (Dynamic IRQ) - **CRITICAL** blocker for e1000
  - #281 (I/O port mgmt) - blocks UACPI
- Drivers:
  - #67 (e1000 network) - blocked by #279
  - #336 (termios) - ready to implement ✅
- Networking stack (future)

**Use this for:**
- Hardware bring-up planning
- Driver development priorities
- Understanding ACPI/UACPI status

**Key Insight:** #279 (Dynamic IRQ) is the critical blocker - unblocks both e1000 driver and completes UACPI interrupt support.

---

### 4. Full Diagram (`issue_dependencies_full.png`) 📊

**Focus:** Complete dependency graph (all issues)

**Size:** 854KB (large and complex)

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
| **Critical** | 174KB | ~15 | TCC/toolchain path |
| **libc** | 293KB | ~30 | Library implementation |
| **Hardware** | 239KB | ~15 | Drivers & ACPI |
| **Full** | 854KB | 100+ | Complete reference |

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

**Last Updated:** 2025-10-21
**Total Issues Tracked:** 100+
**Diagrams:** 4 (3 focused + 1 complete)
