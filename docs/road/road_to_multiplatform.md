# Road to Multi-Platform meniOS

This roadmap outlines the journey to transform meniOS from an x86_64-only operating system into a true multi-platform OS capable of running on multiple architectures. The primary goal is enabling ARM64/AArch64 support while establishing the infrastructure for future architecture ports.

> **Status:** Planning phase (as of 2025-10-15). This represents a major architectural evolution that will span several months of development.
>
> **Update (2025-10-15):** Discovered that Limine natively supports ARM64! Strategy updated to keep Limine as primary bootloader for all architectures, significantly simplifying the migration path.

## Definition of "Multi-Platform"

A release that meets this milestone must satisfy all of the following:

- Boot abstraction layer isolating bootloader protocol details from kernel initialization.
- Bootloader-agnostic kernel that works with Limine (primary) and optionally GRUB/U-Boot.
- Complete ARM64/AArch64 port with feature parity to x86_64 implementation.
- Clean architecture abstraction allowing kernel code to be largely architecture-independent.
- Multi-architecture build system supporting cross-compilation and parallel architecture builds.
- Comprehensive documentation for porting to new architectures.

## Strategic Vision

### Why Multi-Platform?

1. **Hardware Diversity**: Access to ARM servers, embedded systems, mobile platforms, and Apple Silicon.
2. **Portability Validation**: Proves the OS design is not x86-centric and can adapt to different paradigms.
3. **Educational Value**: Demonstrates OS concepts across different architectures and instruction sets.
4. **Future-Proofing**: Infrastructure for RISC-V, ARM32, and other emerging platforms.
5. **Community Growth**: Opens meniOS to developers working on non-x86 hardware.

### Current State (v0.1.0)

- **Single Architecture**: x86_64 only
- **Bootloader**: Limine (x86_64 configuration)
- **Boot Protocol**: Direct Limine protocol integration
- **Architecture Abstraction**: Minimal (x86_64 assumptions throughout)
- **Target Hardware**: QEMU x86_64, physical x86_64 machines

### Target State

- **Multiple Architectures**: x86_64 + ARM64 (with RISC-V as future target)
- **Bootloader**: Limine (multi-platform) as primary, GRUB/U-Boot as optional alternatives
- **Boot Protocol**: Abstract interface with adapters for Limine (x86_64/ARM64), Multiboot2 (optional), device tree
- **Architecture Abstraction**: Clean HAL (Hardware Abstraction Layer) throughout kernel
- **Target Hardware**: QEMU (all architectures), Raspberry Pi 4/5, x86_64 machines, ARM cloud instances

## Key Discovery: Limine Supports ARM64!

After reviewing Limine documentation, we discovered that **Limine natively supports multiple architectures**:

- IA-32 (32-bit x86)
- x86-64 ✅ (currently used)
- **aarch64 (ARM64)** ✅ (perfect for #98!)
- riscv64 (future)
- loongarch64 (future)

**What This Means:**
- No disruptive bootloader migration needed
- Faster timeline (6-9 weeks instead of 14-19 weeks for boot abstraction)
- Less risk (keep using familiar bootloader)
- Same bootloader experience across architectures
- Still create boot abstraction for cleaner code and flexibility

## Milestone Tracker

| Component | Status | Related Issues | Notes |
| --- | --- | --- | --- |
| Boot Abstraction Layer | 📋 Planned | #277 Phase 2 | Generic boot info structure |
| Limine x86_64 Adapter | 📋 Planned | #277 Phase 2 | Convert existing code to abstraction |
| Limine ARM64 Adapter | 📋 Planned | #277 Phase 3 | Linux boot protocol + DT |
| Architecture Abstraction | 📋 Planned | #97, #98 | Clean HAL design |
| ARM64 Memory Management | 📋 Planned | #98 Phase 1 | Page tables, MMU, TLB |
| ARM64 Core Infrastructure | 📋 Planned | #98 Phase 1 | GIC, timers, exceptions |
| ARM64 Process Management | 📋 Planned | #98 Phase 2 | Context switching, syscalls |
| ARM64 Hardware Drivers | 📋 Planned | #98 Phase 3 | UART, GPIO, timers |
| ARM64 Platform Support | 📋 Planned | #98 Phase 3 | Raspberry Pi 4/5 |
| Optional GRUB Support | 💡 Future | #277 Phase 4 | Alternative bootloader |
| Documentation | 📋 Planned | #277 Phase 5, #98 | Porting guide, build docs |

## Key Initiatives

### Initiative 1: Boot Protocol Abstraction (#277)

**Goal**: Create boot abstraction layer with Limine adapters for x86_64 and ARM64.

**Timeline**: 6-9 weeks part-time (vs 14-19 weeks for GRUB migration)

**Phases**:
1. **Research and Planning** (1-2 weeks)
   - Audit Limine protocol usage in kernel
   - Study Limine ARM64 boot protocol
   - Design boot abstraction interface

2. **Boot Abstraction Layer** (2-3 weeks)
   - Define generic `boot_info_t` structure
   - Implement Limine x86_64 adapter
   - Convert kernel subsystems to use abstraction
   - Validate with existing Limine bootloader (no regressions)

3. **Limine ARM64 Support** (2-3 weeks)
   - Implement Limine ARM64 adapter
   - Handle device tree from Limine
   - Parse ARM64-specific boot parameters
   - Test on QEMU ARM64

4. **Optional: GRUB Support** (3-4 weeks, can be deferred)
   - Implement Multiboot2 adapter
   - Add GRUB configuration files
   - Test as alternative bootloader

5. **Documentation and Cleanup** (1 week)
   - Document boot abstraction API
   - Document bootloader adapter interface
   - Update build documentation

**Success Criteria**:
- [x] Boot abstraction layer with zero overhead
- [x] x86_64 boots with abstraction (no regressions)
- [x] ARM64 boot info adapter ready for #98
- [x] Clean separation: no direct Limine calls in kernel code
- [x] Documentation complete and accurate

### Initiative 2: ARM64/AArch64 Port (#98)

**Goal**: Complete ARM64 port with full meniOS functionality using Limine.

**Timeline**: 6-8 months part-time or 3-4 months full-time

**Dependencies**: Boot abstraction layer from #277 (6-9 weeks)

**Phases**:
1. **Basic Bring-up** (6-8 weeks)
   - Limine ARM64 boot protocol implementation
   - Device tree parsing
   - Early console output (UART)
   - Memory management (MMU, page tables)
   - Exception vectors and interrupt handling
   - ARM Generic Timer integration

2. **Core Functionality** (8-10 weeks)
   - Process/thread management
   - Context switching optimization
   - System call implementation
   - Signal handling
   - SMP support (multi-core)

3. **Hardware Support** (6-8 weeks)
   - GIC (Generic Interrupt Controller)
   - Device tree integration
   - Raspberry Pi 4/5 specific drivers
   - UART, GPIO, I2C, SPI
   - Storage (SDHCI/MMC)

4. **Optimization and Validation** (4-6 weeks)
   - NEON/SIMD optimizations
   - Cache tuning
   - Performance benchmarking
   - Real hardware testing
   - Application validation

**Success Criteria**:
- [x] Boot to shell on QEMU ARM64 with Limine
- [x] Boot to shell on Raspberry Pi 4/5 with Limine
- [x] All core syscalls working
- [x] Performance within 10% of x86_64
- [x] Multi-core support functional
- [x] Essential drivers operational

## Architecture Abstraction Strategy

### Layered Approach

```
┌─────────────────────────────────────────┐
│   Kernel Subsystems (arch-independent)  │
│   (VFS, Scheduler, Memory Manager, etc) │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│    Architecture Abstraction Layer       │
│    (Generic interfaces, boot_info_t)    │
└─────────────────────────────────────────┘
                    ↓
┌──────────────────┬──────────────────────┐
│   x86_64 HAL     │     ARM64 HAL        │
│   (low-level)    │     (low-level)      │
└──────────────────┴──────────────────────┘
                    ↓
┌──────────────────┬──────────────────────┐
│  Limine Adapter  │  Limine Adapter      │
│   (x86_64)       │    (ARM64)           │
└──────────────────┴──────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│         Limine Bootloader               │
│    (x86_64, ARM64, RISC-V, etc.)        │
└─────────────────────────────────────────┘
```

### Key Abstractions

1. **Boot Information**
   - Generic memory map
   - Framebuffer details
   - ACPI/Device tree pointers
   - Command line arguments

2. **Memory Management**
   - Page table operations
   - TLB management
   - Cache operations
   - Memory barriers

3. **Interrupt Handling**
   - IRQ enable/disable
   - Interrupt controller init
   - Exception vectors
   - Context save/restore

4. **Process Management**
   - Context switch primitives
   - System call entry/exit
   - Thread state management
   - FPU/SIMD context

5. **Timing and Delays**
   - Timer initialization
   - High-resolution counter
   - Sleep/delay primitives

## Build System Evolution

### Current Build System (x86_64 only)

```makefile
ARCH = x86-64
CC = gcc
CFLAGS = -m64 -march=x86-64
LD = ld -m elf_x86_64
BOOTLOADER = limine
```

### Target Build System (Multi-arch with Limine)

```makefile
# Top-level Makefile supports ARCH= parameter
ARCH ?= x86_64
SUPPORTED_ARCHS = x86_64 aarch64

# Architecture-specific configurations
ifeq ($(ARCH),x86_64)
    CROSS_PREFIX = x86_64-elf
    ARCH_FLAGS = -m64 -march=x86-64
    BOOTLOADER = limine
    BOOT_PROTOCOL = limine
endif

ifeq ($(ARCH),aarch64)
    CROSS_PREFIX = aarch64-linux-gnu
    ARCH_FLAGS = -march=armv8-a
    BOOTLOADER = limine
    BOOT_PROTOCOL = limine-linux  # Limine's Linux boot protocol
endif

# Generic build targets work for all architectures
build: kernel-$(ARCH).elf

# Architecture-specific source paths
ARCH_SRC = src/kernel/arch/$(ARCH)
```

### Build Targets

```bash
# Build for default architecture (x86_64)
make build

# Build for specific architecture
make build ARCH=aarch64

# Build for all architectures
make all-arch

# Run in emulator (architecture-specific QEMU)
make run ARCH=aarch64

# Cross-compile from macOS/Linux
make docker-build ARCH=aarch64
```

## Testing Strategy

### Continuous Integration

| Test Category | x86_64 | ARM64 | Coverage |
| --- | --- | --- | --- |
| Unit Tests | ✅ Host | ✅ Host | 80%+ |
| Boot Tests | ✅ QEMU | 🔄 QEMU | All paths |
| Syscall Tests | ✅ QEMU | 🔄 QEMU | All syscalls |
| Shell Tests | ✅ QEMU | 🔄 QEMU | Interactive |
| Performance | ✅ Bench | 🔄 Bench | Regression |
| Real Hardware | ✅ x86_64 | 🔄 RPi 4 | Weekly |

### Architecture-Specific Testing

**x86_64**:
- QEMU with Limine (BIOS and UEFI boot)
- Real hardware validation
- Performance baseline
- Regression detection

**ARM64**:
- QEMU ARM64 virt machine with Limine
- Raspberry Pi 4/5 validation with Limine
- Cloud instances (AWS Graviton, etc.)
- Performance comparison vs x86_64

### Test Coverage Goals

- Boot paths: 100% (Limine on all architectures)
- Syscalls: 100% (architecture-agnostic interface)
- Kernel subsystems: 80%+ (VFS, scheduler, memory)
- Architecture HAL: 70%+ (platform-specific code)

## Risk Management

### Technical Risks

| Risk | Impact | Probability | Mitigation |
| --- | --- | --- | --- |
| Boot abstraction performance overhead | Medium | Low | Inline functions, zero-cost abstraction |
| ARM64 driver complexity | High | Medium | Start with UART only, incremental addition |
| Limine ARM64 differences from x86_64 | Medium | Low | Research phase, test early |
| Architecture assumptions in code | High | High | Systematic audit, abstraction enforcement |
| Platform-specific bugs | Medium | High | Comprehensive testing on real hardware |
| Timeline overrun | Low | Low | Simpler than GRUB migration |

### Mitigation Strategies

1. **Incremental Development**: Each phase delivers working functionality
2. **Parallel Testing**: Validate both x86_64 and ARM64 continuously
3. **Community Review**: Open PRs for feedback early
4. **Documentation First**: Write docs before implementation
5. **Keep Limine**: No disruptive bootloader migration, lower risk

## Documentation Deliverables

### User Documentation
- [ ] Multi-architecture build guide
- [ ] Limine configuration guide (x86_64 + ARM64)
- [ ] Architecture-specific installation instructions
- [ ] Troubleshooting guide (per-architecture)

### Developer Documentation
- [ ] Boot abstraction API reference
- [ ] Architecture HAL specification
- [ ] Porting guide for new architectures
- [ ] Architecture decision records (ADRs)

### Internal Documentation
- [ ] Limine protocol comparison (x86_64 vs ARM64)
- [ ] Architecture-specific quirks and workarounds
- [ ] Performance tuning guide
- [ ] Testing procedures per architecture

## Success Metrics

### Technical Metrics
- [x] Two fully functional architectures (x86_64, ARM64)
- [x] Boot abstraction layer with minimal overhead (<5%)
- [x] 95%+ kernel code is architecture-agnostic
- [x] Build system supports N architectures with minimal duplication
- [x] Test suite passes on all architectures
- [x] Same bootloader (Limine) across all architectures

### Performance Metrics
- [x] ARM64 boot time competitive with x86_64
- [x] Context switch overhead: ARM64 within 10% of x86_64
- [x] Memory management: ARM64 within 5% of x86_64
- [x] Syscall latency: ARM64 within 10% of x86_64

### Quality Metrics
- [x] Zero regressions on x86_64 during ARM64 development
- [x] Clean architecture separation (no #ifdef pollution)
- [x] Comprehensive test coverage across architectures
- [x] Documentation complete and accurate

### Community Metrics
- [x] Successful deployment on Raspberry Pi 4/5
- [x] Community contributions to ARM64 drivers
- [x] Positive feedback on architecture abstraction design
- [x] Additional architecture ports initiated by community

## Timeline Overview

**Revised timeline with Limine for all architectures:**

```
Month 1-2: Boot Abstraction Layer
  ├─ Research phase (Limine protocols, both architectures)
  ├─ Design boot_info_t interface
  ├─ Implement Limine x86_64 adapter
  ├─ Implement Limine ARM64 adapter
  └─ Validate with no regressions

Month 2-5: ARM64 Basic Bring-up
  ├─ Limine ARM64 boot (device tree)
  ├─ Memory management (MMU)
  ├─ Interrupt handling (GIC)
  └─ Early console (UART)

Month 5-7: ARM64 Core Functionality
  ├─ Process management
  ├─ System calls
  ├─ SMP support
  └─ Signal handling

Month 7-8: ARM64 Hardware & Optimization
  ├─ Device drivers
  ├─ Raspberry Pi support
  ├─ Performance tuning
  └─ Testing and validation

Month 8: Documentation & Release
  ├─ Complete all documentation
  ├─ Final testing sweep
  ├─ Release preparation
  └─ Community announcement
```

**Total Estimated Timeline**: 6-8 months part-time or 3-4 months full-time

**Savings**: 2-3 months compared to GRUB migration approach!

## Next Steps

### Immediate Actions (Week 1-2)
1. Begin Phase 1 of #277 (research and planning)
2. Study Limine protocol specification
3. Study Limine ARM64 boot protocol differences
4. Audit kernel code for architecture assumptions
5. Design boot_info_t structure
6. Set up ARM64 cross-compilation toolchain

### Short Term (Month 1-2)
1. Implement boot abstraction layer
2. Create Limine x86_64 adapter
3. Create Limine ARM64 adapter
4. Validate with existing x86_64 code
5. Test ARM64 boot info parsing

### Medium Term (Month 2-5)
1. Begin ARM64 port (boot + memory)
2. Set up ARM64 CI pipeline
3. Test on QEMU ARM64 with Limine
4. Implement basic ARM64 drivers

### Long Term (Month 5-8)
1. Complete ARM64 feature parity
2. Raspberry Pi 4/5 support with Limine
3. Performance optimization
4. Documentation completion
5. Release multi-platform meniOS

## Platform Firmware: ACPI vs Device Tree

### x86_64: ACPI Required
- **ACPI**: Industry standard for x86/x86_64 platforms
- **UACPI**: Architecture-agnostic ACPI implementation library
- **Status**: 50% complete (#276), sufficient for basic x86_64 needs
- **Priority**: Medium (required for x86_64 ACPI operations)

### ARM64: Device Tree Primary, ACPI Optional

**Raspberry Pi 4/5 (Primary Target)**: Uses **Device Tree**, not ACPI
- Device tree describes hardware topology
- Passed from bootloader (Limine) to kernel
- Parsing needed for device discovery

**ARM64 Servers (Secondary)**: May use ACPI instead of Device Tree
- ARM64 ACPI support is growing but not universal
- Server platforms more likely to use ACPI
- Embedded/SBC platforms typically use Device Tree

### Multi-Platform UACPI Strategy

**UACPI is architecture-agnostic** and works on:
- x86_64: Full ACPI support (primary use case)
- ARM64: Optional ACPI support (servers, cloud instances)

**For ARM64 Raspberry Pi**: Device Tree parsing is more important than completing UACPI infrastructure.

**Recommendation**:
- Complete UACPI for x86_64 needs (current progress sufficient)
- Defer medium/low priority UACPI infrastructure (#278-#281) until needed
- Focus ARM64 effort on Device Tree parsing for Raspberry Pi support

## Recent Progress (2025-10-15)

### UACPI Integration (#276)

Significant progress on UACPI kernel interface implementation:
- **50% complete**: 8 out of 16 functions implemented
- **Production-ready**: Thread IDs, events (semaphore-backed), sleep, tick counter (TSC-based)
- **Remaining work**: Now tracked as infrastructure issues (#278, #279, #280, #281)
- **Current status**: Sufficient for basic x86_64 ACPI operations

The remaining UACPI functions require substantial kernel infrastructure:

1. **#278 - Kernel Work Queue System** (Medium priority, 8-11 weeks)
   - Deferred work execution infrastructure
   - Required for: `uacpi_kernel_schedule_work()`, `uacpi_kernel_wait_for_work_completion()`
   - Benefits beyond ACPI: Driver infrastructure, interrupt bottom-halves

2. **#279 - Dynamic IRQ Handler Registration** (Medium priority, 8-10 weeks)
   - Runtime interrupt handler management
   - Required for: `uacpi_kernel_install_interrupt_handler()`, `uacpi_kernel_uninstall_interrupt_handler()`
   - Benefits beyond ACPI: Shared interrupts, ACPI SCI, device drivers

3. **#280 - PCI Configuration Space Access** (Low priority, 8-10 weeks)
   - PCI device configuration API
   - Required for: `uacpi_kernel_pci_read()`, `uacpi_kernel_pci_write()`
   - Benefits beyond ACPI: PCI enumeration, device power management

4. **#281 - I/O Port Resource Management** (Low priority, 7 weeks)
   - I/O port allocation and tracking
   - Required for: `uacpi_kernel_io_map()`, `uacpi_kernel_io_unmap()`, etc.
   - Benefits beyond ACPI: Resource conflict detection, debugging

### Infrastructure Dependencies

These kernel infrastructure pieces are valuable beyond just ACPI:
- **Work queues**: Generic deferred work for all subsystems
- **IRQ management**: Dynamic handler registration for all drivers
- **PCI access**: Foundation for PCI driver framework
- **I/O port management**: Resource tracking across the kernel

While these are medium-to-low priority for multi-platform support, they represent important kernel capabilities that should be considered when planning the overall architecture.

## Related Issues

### Multi-Platform Core
- **#277**: Create boot protocol abstraction layer for multi-platform support
- **#98**: Port meniOS to ARM64/AArch64 architecture
- **#97**: Architecture abstraction layer (if exists)

### Infrastructure (Supporting ACPI and Multi-Platform)
- **#276**: UACPI kernel interface implementation (50% complete)
- **#278**: Kernel work queue system (Medium priority)
- **#279**: Dynamic IRQ handler registration (Medium priority)
- **#280**: PCI configuration space access (Low priority)
- **#281**: I/O port resource management (Low priority)

## References

- [Limine Protocol Specification](https://codeberg.org/Limine/limine-protocol/src/branch/trunk/PROTOCOL.md)
- [Limine Repository](https://codeberg.org/Limine/Limine)
- [Limine ARM64 Support](https://codeberg.org/Limine/Limine#supported-architectures)
- [Linux ARM64 Boot Protocol](https://www.kernel.org/doc/Documentation/arm64/booting.txt)
- [Device Tree Specification](https://www.devicetree.org/)
- [ARM Architecture Reference Manual](https://developer.arm.com/documentation/)
- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) (for optional GRUB support)

---

**Document Status**: Living document, updated as milestones are reached.

**Last Updated**: 2025-10-15 (Updated to reflect Limine ARM64 support discovery)

**Milestone**: multi-platform (https://github.com/pbalduino/menios/milestone/5)
